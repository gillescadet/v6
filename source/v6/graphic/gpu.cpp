/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/string.h>
#include <v6/core/thread.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

static const u32 EVENT_BUFFER_COUNT			= 3;
static const u32 EVENT_MAX_COUNT			= 32;
static const u32 EVENT_NAME_MAX_SIZE		= 64;
static const u32 EVENT_STACK_MAX_SIZE		= 16;
static const u32 EVENT_TIMING_FRAME_COUNT	= 32;
static const u32 VERTEX_INPUT_MAX_COUNT		= 6;

struct GPUEventContext_s
{
	char				eventNames[EVENT_MAX_COUNT][EVENT_NAME_MAX_SIZE];
	bool				eventProfiles[EVENT_MAX_COUNT];
	u32					eventCount;

	struct
	{
		GPUQuery_s		frequency;
		GPUQuery_s		begins[EVENT_MAX_COUNT];
		GPUQuery_s		ends[EVENT_MAX_COUNT];
	}					queries[EVENT_BUFFER_COUNT];

	struct
	{
		u32				frameID;
		u32				stackIDs[EVENT_STACK_MAX_SIZE];
		u32				stackSize;
	}					state;

	struct
	{
		GPUEventID_t	ids[EVENT_MAX_COUNT];
		u8				depths[EVENT_MAX_COUNT];
		u32				count;
	}					hierarchies[EVENT_BUFFER_COUNT];

	struct
	{
		u32				durations[EVENT_MAX_COUNT][EVENT_TIMING_FRAME_COUNT];
		u64				durationSums[EVENT_MAX_COUNT];
	}					timings;

	GPUEventDuration_s	eventDurations[EVENT_MAX_COUNT];
};

struct BasicVertex_s
{
	Vec3 position;
	Color_s color;
};

ID3D11Device*						g_device = nullptr;
ID3D11DeviceContext*				g_deviceContext = nullptr;
static ID3DUserDefinedAnnotation*	s_userDefinedAnnotation = nullptr;

GPUEventContext_s					s_eventContext = {};
GPUSurfaceContext_s					s_surfaceContext = {};
GPUShaderContext_s					s_shaderContext = {};

static u32 s_gpuMemory = 0;

static u32 DXGIFormat_Size( DXGI_FORMAT format )
{
	switch( format )
	{	
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_SNORM:
		return 2;
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_TYPELESS:
		return 4;
	case DXGI_FORMAT_R32G32_FLOAT:
		return 8;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return 16;
	default:
		V6_ASSERT_NOT_SUPPORTED();
		return 0;
	}
}

GPUEventID_t GPUEvent_Register( const char* eventName, bool profile )
{
	if ( s_eventContext.eventCount == 0 )
	{
		strcpy_s( s_eventContext.eventNames[0], EVENT_NAME_MAX_SIZE-1, "Frame" );
		s_eventContext.eventProfiles[0] = true;
		s_eventContext.eventCount = 1;
	}

	V6_ASSERT( s_eventContext.eventCount < EVENT_MAX_COUNT );
	const GPUEventID_t eventID = s_eventContext.eventCount;

	strcpy_s( s_eventContext.eventNames[eventID], EVENT_NAME_MAX_SIZE-1, eventName );
	s_eventContext.eventProfiles[eventID] = profile;

	++s_eventContext.eventCount;
	return eventID;
}

void GPUEvent_Begin( GPUEventID_t eventID )
{
	V6_ASSERT( eventID < s_eventContext.eventCount );

	const u32 bufferID = s_eventContext.state.frameID % EVENT_BUFFER_COUNT;
	const u32 depth = s_eventContext.state.stackSize;
	
	V6_ASSERT( depth < EVENT_STACK_MAX_SIZE );
	s_eventContext.state.stackIDs[s_eventContext.state.stackSize] = eventID;
	++s_eventContext.state.stackSize;

	if ( s_eventContext.eventProfiles[eventID] )
	{
		{
			if ( s_eventContext.queries[bufferID].begins[eventID].query == nullptr )
				GPUQuery_CreateTimeStamp( &s_eventContext.queries[bufferID].begins[eventID] );

			GPUQuery_WriteTimeStamp( &s_eventContext.queries[bufferID].begins[eventID] );
		}

		{
			const u32 eventRank = s_eventContext.hierarchies[bufferID].count;
			s_eventContext.hierarchies[bufferID].ids[eventRank] = eventID;
			s_eventContext.hierarchies[bufferID].depths[eventRank] = depth;
			++s_eventContext.hierarchies[bufferID].count;
		}
	}

	{
		const u32 len = (u32)strlen( s_eventContext.eventNames[eventID] );
		WCHAR eventNameW[EVENT_NAME_MAX_SIZE+1];
		MultiByteToWideChar( CP_ACP, 0, s_eventContext.eventNames[eventID], len, eventNameW, EVENT_NAME_MAX_SIZE );
		eventNameW[len] = 0;
		
		s_userDefinedAnnotation->BeginEvent( eventNameW );
	}
}

void GPUEvent_BeginFrame( u32 frameID )
{
	if ( s_eventContext.eventCount == 0 )
		return;

	const u32 bufferID = frameID % EVENT_BUFFER_COUNT;

	{
		if ( s_eventContext.queries[bufferID].frequency.query == nullptr )
			GPUQuery_CreateTimeStampDisjoint( &s_eventContext.queries[bufferID].frequency );

		GPUQuery_BeginTimeStampDisjoint( &s_eventContext.queries[bufferID].frequency );
	}

	V6_ASSERT( s_eventContext.state.stackSize == 0 );
	s_eventContext.state.frameID = frameID;

	s_eventContext.hierarchies[bufferID].count = 0;

	GPUEvent_Begin( 0 );

	V6_ASSERT( s_eventContext.state.stackSize == 1 );
	V6_ASSERT( s_eventContext.state.stackIDs[0] == 0 );
}

void GPUEvent_End()
{
	s_userDefinedAnnotation->EndEvent();

	V6_ASSERT( s_eventContext.state.stackSize > 0 );
	--s_eventContext.state.stackSize;
	const GPUEventID_t eventID = s_eventContext.state.stackIDs[s_eventContext.state.stackSize];

	if ( s_eventContext.eventProfiles[eventID] )
	{
		const u32 bufferID = s_eventContext.state.frameID % EVENT_BUFFER_COUNT;
		
		if ( s_eventContext.queries[bufferID].ends[eventID].query == nullptr )
			GPUQuery_CreateTimeStamp( &s_eventContext.queries[bufferID].ends[eventID] );

		GPUQuery_WriteTimeStamp( &s_eventContext.queries[bufferID].ends[eventID] );
	}
}

void GPUEvent_EndFrame()
{
	if ( s_eventContext.eventCount == 0 )
		return;

	V6_ASSERT( s_eventContext.state.stackSize == 1 );
	V6_ASSERT( s_eventContext.state.stackIDs[0] == 0 );
	
	GPUEvent_End();

	V6_ASSERT( s_eventContext.state.stackSize == 0 );

	{
		const u32 bufferID = s_eventContext.state.frameID % EVENT_BUFFER_COUNT;
		
		GPUQuery_EndTimeStampDisjoint( &s_eventContext.queries[bufferID].frequency );
	}
}

u32 GPUEvent_UpdateDurations( GPUEventDuration_s** eventDurations )
{
	const u32 prevBufferID = (s_eventContext.state.frameID + 1) % EVENT_BUFFER_COUNT;
	if ( s_eventContext.hierarchies[prevBufferID].count == 0 )
		return 0;

	GPUQuery_s* frequency = &s_eventContext.queries[prevBufferID].frequency;
	if ( !GPUQuery_ReadTimeStampDisjoint( frequency ) )
		return 0;

	const u32 eventCount = s_eventContext.hierarchies[prevBufferID].count;
	*eventDurations = s_eventContext.eventDurations;

	const u32 timingFrameID = s_eventContext.state.frameID % EVENT_TIMING_FRAME_COUNT;
	for ( u32 eventRank = 0; eventRank < eventCount; ++eventRank )
	{
		const GPUEventID_t eventID = s_eventContext.hierarchies[prevBufferID].ids[eventRank];

		GPUQuery_ReadTimeStamp( &s_eventContext.queries[prevBufferID].begins[eventID] );
		GPUQuery_ReadTimeStamp( &s_eventContext.queries[prevBufferID].ends[eventID] );

		const float time = GPUQuery_GetElpasedTime( &s_eventContext.queries[prevBufferID].begins[eventID], &s_eventContext.queries[prevBufferID].ends[eventID], frequency );
		const u32 timeUS = (u32)(Min( time, 1.0f ) * 1000000.0f);

		s_eventContext.timings.durationSums[eventID] -= s_eventContext.timings.durations[eventID][timingFrameID];
		s_eventContext.timings.durations[eventID][timingFrameID] = timeUS;
		s_eventContext.timings.durationSums[eventID] += timeUS;

		(*eventDurations)[eventRank].id = eventID;
		(*eventDurations)[eventRank].depth = s_eventContext.hierarchies[prevBufferID].depths[eventRank];
		(*eventDurations)[eventRank].name = s_eventContext.eventNames[eventID];
		(*eventDurations)[eventRank].avgDurationUS = (u32)(s_eventContext.timings.durationSums[eventID] / EVENT_TIMING_FRAME_COUNT);
		(*eventDurations)[eventRank].curDurationUS = timeUS;
	}

	s_eventContext.hierarchies[prevBufferID].count = 0;

	return eventCount;
}

static void GPUEventContext_Release()
{
	for ( GPUEventID_t eventID = 0; eventID < s_eventContext.eventCount; ++eventID )
	{
		for ( u32 bufferID = 0; bufferID < EVENT_BUFFER_COUNT; ++bufferID )
		{
			if ( s_eventContext.queries[bufferID].begins[eventID].query )
				GPUQuery_Release( &s_eventContext.queries[bufferID].begins[eventID] );
			if ( s_eventContext.queries[bufferID].ends[eventID].query )
				GPUQuery_Release( &s_eventContext.queries[bufferID].ends[eventID] );
		}
	}

	for ( u32 bufferID = 0; bufferID < EVENT_BUFFER_COUNT; ++bufferID )
	{
		if ( s_eventContext.queries[bufferID].frequency.query )
			GPUQuery_Release( &s_eventContext.queries[bufferID].frequency );
	}

	memset( &s_eventContext, 0, sizeof( s_eventContext ) );
}

void GPUResource_LogMemory( const char* res, u32 size, const char* name )
{
	if ( DivMB( size ) >= 1 )
		V6_MSG( "%-16s %-30s: %8s MB\n", res, name, String_FormatInteger( DivMB( size ) ) );
	Atomic_Add( &s_gpuMemory, size );
}

void GPUResource_LogMemoryUsage()
{
	V6_MSG( "%-16s %-30s: %8s MB\n", "GPU", "total", String_FormatInteger( DivMB( s_gpuMemory ) ) );
}

void GPUCompute_CreateFromSource( GPUCompute_s* compute, const void* source, u32 sourceSize )
{
	V6_ASSERT_D3D11( g_device->CreateComputeShader( source, sourceSize, nullptr, &compute->m_computeShader ) );
}

bool GPUCompute_CreateFromFile( GPUCompute_s* compute, const char* cs, IAllocator* allocator )
{
	Stack stack( allocator, 64 * 1024 );

	void* csBytecode = nullptr;
	const int csBytecodeSize = FileSystem_ReadFile( cs, &csBytecode, &stack );
	if ( csBytecodeSize <= 0 )
	{
		V6_ERROR( "File %s not found!\n", cs );
		return false;
	}

	GPUCompute_CreateFromSource( compute, csBytecode, csBytecodeSize );

	return true;
}

void GPUCompute_Release( GPUCompute_s* compute )
{	
	V6_RELEASE_D3D11( compute->m_computeShader );
}

void GPUCompute_Dispatch( GPUCompute_s* compute, u32 groupX, u32 groupY, u32 groupZ )
{
	g_deviceContext->CSSetShader( compute->m_computeShader, nullptr, 0 );
	g_deviceContext->Dispatch( groupX, groupY, groupZ );
}

void GPUCompute_DispatchIndirect( GPUCompute_s* compute, GPUBuffer_s* bufferArgs, u32 offsetArgs )
{
	g_deviceContext->CSSetShader( compute->m_computeShader, nullptr, 0 );
	g_deviceContext->DispatchIndirect( bufferArgs->buf, offsetArgs );
}

void GPUShader_CreateFromSource( GPUShader_s* shader, const void* sourceVS, u32 sourceSizeVS, const void* sourcePS, u32 sourceSizePS, u32 vertexFormat )
{
	V6_ASSERT_D3D11( g_device->CreateVertexShader( sourceVS, sourceSizeVS, nullptr, &shader->m_vertexShader ) );
	V6_ASSERT_D3D11( g_device->CreatePixelShader( sourcePS, sourceSizePS, nullptr, &shader->m_pixelShader ) );
	
	D3D11_INPUT_ELEMENT_DESC idesc[VERTEX_INPUT_MAX_COUNT] = {};

	int stride = 0;
	int inputCount = 0;
		
	if ( vertexFormat & VERTEX_FORMAT_POSITION )
	{
		V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

		idesc[inputCount].SemanticName = "POSITION";
		idesc[inputCount].SemanticIndex = 0;
		idesc[inputCount].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		idesc[inputCount].InputSlot = 0;
		idesc[inputCount].AlignedByteOffset = stride;
		idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[inputCount].InstanceDataStepRate = 0;

		stride += 12;
		++inputCount;
	}

	if ( vertexFormat & VERTEX_FORMAT_COLOR )
	{
		V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

		idesc[inputCount].SemanticName = "COLOR";
		idesc[inputCount].SemanticIndex = 0;
		idesc[inputCount].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		idesc[inputCount].InputSlot = 0;
		idesc[inputCount].AlignedByteOffset = stride;
		idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[inputCount].InstanceDataStepRate = 0;

		stride += 4;
		++inputCount;
	}

	const static DXGI_FORMAT widthToFloatFormats[] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };

	if ( vertexFormat & VERTEX_FORMAT_USER0_MASK )
	{
		V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

		idesc[inputCount].SemanticName = "USER";
		idesc[inputCount].SemanticIndex = 0;
		const u32 width = ( vertexFormat & VERTEX_FORMAT_USER0_MASK ) >> VERTEX_FORMAT_USER0_SHIFT;
		V6_ASSERT( width >= 1 && width <= 4 );
		idesc[inputCount].Format = widthToFloatFormats[width];
		idesc[inputCount].InputSlot = 0;
		idesc[inputCount].AlignedByteOffset = stride;
		idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[inputCount].InstanceDataStepRate = 0;

		stride += 4 * width;
		++inputCount;
	}

	if ( vertexFormat & VERTEX_FORMAT_USER1_MASK )
	{
		V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

		idesc[inputCount].SemanticName = "USER";
		idesc[inputCount].SemanticIndex = 1;
		const u32 width = ( vertexFormat & VERTEX_FORMAT_USER1_MASK ) >> VERTEX_FORMAT_USER1_SHIFT;
		V6_ASSERT( width >= 1 && width <= 4 );
		idesc[inputCount].Format = widthToFloatFormats[width];
		idesc[inputCount].InputSlot = 0;
		idesc[inputCount].AlignedByteOffset = stride;
		idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[inputCount].InstanceDataStepRate = 0;

		stride += 4 * width;
		++inputCount;
	}

	if ( vertexFormat & VERTEX_FORMAT_USER2_MASK )
	{
		V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

		idesc[inputCount].SemanticName = "USER";
		idesc[inputCount].SemanticIndex = 2;
		const u32 width = ( vertexFormat & VERTEX_FORMAT_USER2_MASK ) >> VERTEX_FORMAT_USER2_SHIFT;
		V6_ASSERT( width >= 1 && width <= 4 );
		idesc[inputCount].Format = widthToFloatFormats[width];
		idesc[inputCount].InputSlot = 0;
		idesc[inputCount].AlignedByteOffset = stride;
		idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[inputCount].InstanceDataStepRate = 0;

		stride += 4 * width;
		++inputCount;
	}

	if ( vertexFormat & VERTEX_FORMAT_USER3_MASK )
	{
		V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

		idesc[inputCount].SemanticName = "USER";
		idesc[inputCount].SemanticIndex = 3;
		const u32 width = ( vertexFormat & VERTEX_FORMAT_USER3_MASK ) >> VERTEX_FORMAT_USER3_SHIFT;
		V6_ASSERT( width >= 1 && width <= 4 );
		idesc[inputCount].Format = widthToFloatFormats[width];
		idesc[inputCount].InputSlot = 0;
		idesc[inputCount].AlignedByteOffset = stride;
		idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[inputCount].InstanceDataStepRate = 0;

		stride += 4 * width;
		++inputCount;
	}

	V6_ASSERT_D3D11( g_device->CreateInputLayout( idesc, inputCount, sourceVS, sourceSizeVS, &shader->m_inputLayout ) );

	shader->m_vertexFormat = vertexFormat;
}

bool GPUShader_Create( GPUShader_s* shader, const char* vs, const char* ps, u32 vertexFormat, IAllocator* allocator )
{
	Stack stack( allocator, 64 * 1024 );

	void* vsBytecode = nullptr;
	const int vsBytecodeSize = FileSystem_ReadFile( vs, &vsBytecode, &stack );
	if ( vsBytecodeSize <= 0 )
	{
		V6_ERROR( "Unable to read file %s!\n", vs );
		return false;
	}

	void* psBytecode = nullptr;
	const int psBytecodeSize = FileSystem_ReadFile( ps, &psBytecode, &stack );
	if ( psBytecodeSize <= 0 )
	{
		V6_ERROR( "Unable to read file %s!\n", ps );
		return false;
	}

	GPUShader_CreateFromSource( shader, vsBytecode, vsBytecodeSize, psBytecode, psBytecodeSize, vertexFormat );

	return true;
}

void GPUShader_Release( GPUShader_s* shader )
{
	V6_RELEASE_D3D11( shader->m_inputLayout );
	V6_RELEASE_D3D11( shader->m_vertexShader );
	V6_RELEASE_D3D11( shader->m_pixelShader );
}

void GPUBuffer_CreateIndirectArgs( GPUBuffer_s* buffer, u32 count, u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	buffer->size = count * sizeof( u32 );
	buffer->flags = flags;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

void GPUBuffer_CreateIndirectArgsWithStaticData( GPUBuffer_s* buffer, const void* data, u32 count, u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	buffer->size = count * sizeof( u32 );
	buffer->flags = flags;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, &dataDesc, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	V6_ASSERT( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) == 0 );

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

void GPUBuffer_CreateTyped( GPUBuffer_s* buffer, DXGI_FORMAT format, u32 count, u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	buffer->size = count * DXGIFormat_Size( format );
	buffer->flags = flags;

	const bool isCPUWrite = (flags & (GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE | GPUBUFFER_CREATION_FLAG_UPDATE)) != 0;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = (flags & GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE) != 0 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( !isCPUWrite )
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = (flags & GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE) != 0 ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	if ( !isCPUWrite )
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

void GPUBuffer_CreateTypedWithStaticData( GPUBuffer_s* buffer, const void* data, DXGI_FORMAT format, u32 count, u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	buffer->size = count * DXGIFormat_Size( format );
	buffer->flags = flags;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, &dataDesc, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	V6_ASSERT( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) == 0 );

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

void GPUBuffer_CreateStructured( GPUBuffer_s* buffer, u32 elementSize, u32 count, u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	buffer->size = count * elementSize;
	buffer->flags = flags;

	const bool isCPUWrite = (flags & (GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE | GPUBUFFER_CREATION_FLAG_UPDATE)) != 0;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = (flags & GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE) != 0 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( !isCPUWrite )
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = (flags & GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE) != 0 ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}
	
	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	if ( !isCPUWrite )
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

void GPUBuffer_CreateStructuredWithStaticData( GPUBuffer_s* buffer, const void* data, u32 elementSize, u32 count, u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	buffer->size = count * elementSize;
	buffer->flags = flags;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer->size;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", buffer->size, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

void GPUBuffer_Release( GPUBuffer_s* buffer )
{
	V6_RELEASE_D3D11( buffer->buf );
	if ( buffer->staging )
		V6_RELEASE_D3D11( buffer->staging );
	V6_RELEASE_D3D11( buffer->srv );
	if ( buffer->uav )
		V6_RELEASE_D3D11( buffer->uav );
}

const void* GPUBuffer_MapReadBack( GPUBuffer_s* buffer )
{
	g_deviceContext->CopyResource( buffer->staging, buffer->buf );

	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( g_deviceContext->Map( buffer->staging, 0, D3D11_MAP_READ, 0, &res ) );
	return res.pData;
}

void GPUBuffer_UnmapReadBack( GPUBuffer_s* buffer )
{
	g_deviceContext->Unmap( buffer->staging, 0 );
}

void GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const void* srcData, u32 sizeOfSrcElem, u32 srcCount )
{
	V6_ASSERT( dstOffset * sizeOfSrcElem + srcCount * sizeOfSrcElem <= dstBuffer->size );
	
	const bool isCPUWrite = (dstBuffer->flags & (GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE | GPUBUFFER_CREATION_FLAG_UPDATE)) != 0;
	V6_ASSERT( isCPUWrite );

	if ( dstBuffer->flags & GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE )
	{
		D3D11_MAPPED_SUBRESOURCE res;
		const HRESULT mapResult = g_deviceContext->Map( dstBuffer->buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &res );
		if ( mapResult == S_OK )
		{
			memcpy( (u8*)res.pData + dstOffset * sizeOfSrcElem, srcData, srcCount * sizeOfSrcElem );
			g_deviceContext->Unmap( dstBuffer->buf, 0 );
		}
		else
		{
			V6_ERROR( "ID3D11DeviceContext::Map() failed with error code 0x%08X\n", mapResult );
		}
	}
	else
	{
		V6_ASSERT( dstBuffer->flags & GPUBUFFER_CREATION_FLAG_UPDATE );

		D3D11_BOX dstBox;
		dstBox.left = dstOffset * sizeOfSrcElem;
		dstBox.right = (dstOffset + srcCount) * sizeOfSrcElem;
		dstBox.front = 0;
		dstBox.back = 1;
		dstBox.top = 0;
		dstBox.bottom = 1;

		const u32 size = dstBox.right - dstBox.left;
		g_deviceContext->UpdateSubresource( dstBuffer->buf, 0, &dstBox, srcData, size, size );
	}
}

void GPUColorRenderTarget_Create( GPUColorRenderTarget_s* colorRenderTarget, u32 width, u32 height, u32 sampleCount, bool bindable, bool writable, const char* name )
{
	V6_ASSERT( sampleCount > 0 );

	memset( colorRenderTarget, 0, sizeof( *colorRenderTarget ) );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = sampleCount;
		texDesc.SampleDesc.Quality = sampleCount == 1 ? 0 : 16;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		if ( bindable )
			texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		if ( writable )
			texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( g_device->CreateTexture2D( &texDesc, nullptr, &colorRenderTarget->tex) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = sampleCount == 1 ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DMS;
		viewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( g_device->CreateRenderTargetView( colorRenderTarget->tex, &viewDesc, &colorRenderTarget->rtv ) );
	}

	if ( bindable )
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = sampleCount == 1 ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( colorRenderTarget->tex, &viewDesc, &colorRenderTarget->srv ) );
	}

	if ( writable )
	{
		V6_ASSERT( sampleCount == 1 );

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( colorRenderTarget->tex, &uavDesc, &colorRenderTarget->uav ) );
	}
}

void GPUColorRenderTarget_Copy( GPUColorRenderTarget_s* dstColorRenderTarget, GPUColorRenderTarget_s* srcColorRenderTarget )
{
	g_deviceContext->CopyResource( dstColorRenderTarget->tex, srcColorRenderTarget->tex );
}

void GPUColorRenderTarget_Release( GPUColorRenderTarget_s* colorRenderTarget )
{
	V6_RELEASE_D3D11( colorRenderTarget->tex );
	V6_RELEASE_D3D11( colorRenderTarget->rtv );
	if ( colorRenderTarget->srv )
		V6_RELEASE_D3D11( colorRenderTarget->srv );
	if ( colorRenderTarget->uav )
		V6_RELEASE_D3D11( colorRenderTarget->uav );
}

void GPUDepthRenderTarget_Create( GPUDepthRenderTarget_s* depthRenderTarget, u32 width, u32 height, u32 sampleCount, bool bindable, const char* name )
{
	memset( depthRenderTarget, 0, sizeof( *depthRenderTarget ) );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		texDesc.SampleDesc.Count = sampleCount;
		texDesc.SampleDesc.Quality = sampleCount == 1 ? 0 : 16;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		if ( bindable )
			texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( g_device->CreateTexture2D( &texDesc, nullptr, &depthRenderTarget->tex ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = sampleCount == 1 ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DMS;
		viewDesc.Flags = 0;
		viewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( g_device->CreateDepthStencilView( depthRenderTarget->tex, &viewDesc, &depthRenderTarget->dsv ) );
	}

	if ( bindable )
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = sampleCount == 1 ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( depthRenderTarget->tex, &viewDesc, &depthRenderTarget->srv ) );
	}
}

void GPUDepthRenderTarget_Release( GPUDepthRenderTarget_s* depthRenderTarget )
{
	V6_RELEASE_D3D11( depthRenderTarget->tex );
	V6_RELEASE_D3D11( depthRenderTarget->dsv );
	if ( depthRenderTarget->srv )
		V6_RELEASE_D3D11( depthRenderTarget->srv );
}

void GPUTexture2D_Create( GPUTexture2D_s* tex, u32 width, u32 height, Color_s* pixels, bool mipmap, const char* name )
{
	mipmap = mipmap && IsPowOfTwo( width ) && IsPowOfTwo( height );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = mipmap ? 0 : 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( mipmap )
			texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		if ( mipmap )
			texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		D3D11_SUBRESOURCE_DATA data[16] = {};
		
		u32 pixelCount = 0;
		for ( u32 mip = 0; mip < 16; ++mip )
		{
			data[mip].pSysMem = pixels;
			data[mip].SysMemPitch = width * DXGIFormat_Size( texDesc.Format );
			data[mip].SysMemSlicePitch = width * height * DXGIFormat_Size( texDesc.Format );

			pixelCount += width * height;

			if ( !mipmap || (width == 1 && height == 1))
				break;

			if ( width > 1 )
				width >>= 1;

			if ( height > 1 )
				height >>= 1;
		}

		V6_ASSERT( !mipmap || width == 1 );
		V6_ASSERT( !mipmap || height == 1 );
		V6_ASSERT_D3D11( g_device->CreateTexture2D( &texDesc, data, &tex->tex ) );

		GPUResource_LogMemory( "Texture2D", pixelCount * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = -1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}
	
	tex->uav = nullptr;
	tex->mipmapState = mipmap ? GPUTEXTURE_MIPMAP_STATE_REQUIRED : GPUTEXTURE_MIPMAP_STATE_NONE;
}

void GPUTexture2D_CreateCompressed( GPUTexture2D_s* tex, u32 width, u32 height, void* compressedData, bool mipmap, const char* name )
{
	V6_ASSERT( IsPowOfTwo( width ) && IsPowOfTwo( height ) && width >= 4 && height >= 4 );

	u32 mipCount;

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_BC1_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA data[16] = {};
		
		u32 mip = 0;
		u32 size = 0;
		for (;;)
		{
			data[mip].pSysMem = (u8*)compressedData + size;
			data[mip].SysMemPitch = (width / 4) * 8;
			data[mip].SysMemSlicePitch = (width / 4) * (height / 4) * 8;

			size += data[mip].SysMemSlicePitch;

			if ( !mipmap || (width == 4 || height == 4) || mip == 15 )
				break;

			width >>= 1;
			height >>= 1;

			++mip;
		}

		mipCount = mip + 1; 
		texDesc.MipLevels = mipCount;

		V6_ASSERT( !mipmap || width == 4 || height == 4 );

		V6_ASSERT_D3D11( g_device->CreateTexture2D( &texDesc, data, &tex->tex ) );

		GPUResource_LogMemory( "Texture2D", size * texDesc.ArraySize, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_BC1_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = mipCount;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}
	
	tex->uav = nullptr;
	tex->mipmapState = mipmap ? GPUTEXTURE_MIPMAP_STATE_GENERATED : GPUTEXTURE_MIPMAP_STATE_NONE;
}

void GPUTexture2D_CreateRW( GPUTexture2D_s* tex, u32 width, u32 height, const char* name )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( g_device->CreateTexture2D( &texDesc, nullptr, &tex->tex ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( tex->tex, &uavDesc, &tex->uav ) );
	}

	tex->mipmapState = GPUTEXTURE_MIPMAP_STATE_NONE;
}

void GPUTexture2D_Release( GPUTexture2D_s* tex )
{
	V6_RELEASE_D3D11( tex->tex );
	V6_RELEASE_D3D11( tex->srv );
	if ( tex->uav )
		V6_RELEASE_D3D11( tex->uav );
}

void GPUConstantBuffer_Create( GPUConstantBuffer_s* buffer, u32 sizeOfStruct, const char* name )
{
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = sizeOfStruct;
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.MiscFlags = 0;
	bufDesc.StructureByteStride = 0;

	V6_ASSERT_D3D11( g_device->CreateBuffer( &bufDesc, nullptr, &buffer->buf ) );
	GPUResource_LogMemory( "ConstantBuffer", bufDesc.ByteWidth, name );
}

void GPUConstantBuffer_Release( GPUConstantBuffer_s* buffer )
{
	V6_RELEASE_D3D11( buffer->buf );
}

void* GPUConstantBuffer_MapWrite( GPUConstantBuffer_s* buffer )
{
	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( g_deviceContext->Map( buffer->buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );
	return res.pData;
}

void GPUConstantBuffer_UnmapWrite( GPUConstantBuffer_s* buffer )
{
	g_deviceContext->Unmap( buffer->buf, 0 );
}

void GPUQuery_CreateTimeStamp( GPUQuery_s* query )
{
	query->data = 0;

	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( g_device->CreateQuery( &queryDesc, &query->query ) );
}

void GPUQuery_CreateTimeStampDisjoint( GPUQuery_s* query )
{
	query->data = 0;

	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( g_device->CreateQuery( &queryDesc, &query->query ) );
}

void GPUQuery_BeginTimeStampDisjoint( GPUQuery_s* query )
{
	V6_ASSERT( query->data != (u64)-1 );
	query->data = (u64)-1;

	g_deviceContext->Begin( query->query );
}

void GPUQuery_EndTimeStampDisjoint( GPUQuery_s* query )
{
	V6_ASSERT( query->data == (u64)-1 );
	query->data = 0;

	g_deviceContext->End( query->query );
}

bool GPUQuery_ReadTimeStampDisjoint( GPUQuery_s* query )
{
	V6_ASSERT( query->data != (u64)-1 );

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint = {};
	if ( g_deviceContext->GetData( query->query, &timestampDisjoint, sizeof( timestampDisjoint ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) != S_OK )
		return false;
	if ( timestampDisjoint.Disjoint || timestampDisjoint.Frequency == 0 )
		return false;
	query->data = timestampDisjoint.Frequency;
	return true;
}

void GPUQuery_WriteTimeStamp( GPUQuery_s* query )
{
	query->data = 0;

	g_deviceContext->End( query->query );
}

bool GPUQuery_ReadTimeStamp( GPUQuery_s* query )
{
	return g_deviceContext->GetData( query->query, &query->data, sizeof( query->data ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK;
}

float GPUQuery_GetElpasedTime( const GPUQuery_s* queryStart, const GPUQuery_s* queryEnd, const GPUQuery_s* queryDisjoint )
{
	V6_ASSERT( queryStart->data != (u64)-1 );
	V6_ASSERT( queryEnd->data != (u64)-1 );
	V6_ASSERT( queryDisjoint->data != 0 );
	V6_ASSERT( queryDisjoint->data != (u64)-1 );
	return (float)(queryEnd->data - queryStart->data) / queryDisjoint->data;
}

void GPUQuery_Release( GPUQuery_s* query )
{
	V6_RELEASE_D3D11( query->query );
}

void GPUMesh_UpdateVertices( GPUMesh_s* mesh, const void* vertices )
{
	V6_ASSERT( mesh->m_vertexBuffer );

	D3D11_BOX box;
	box.left = 0;
	box.right = mesh->m_vertexCount * mesh->m_vertexSize;
	box.front = 0;
	box.back = 1;
	box.top = 0;
	box.bottom = 1;
		
	g_deviceContext->UpdateSubresource( mesh->m_vertexBuffer, 0, &box, vertices, box.right, box.right );
}

void GPUMesh_Create( GPUMesh_s* mesh, const void* vertices, u32 vertexCount, u32 vertexSize, u32 vertexFormat, const void* indices, u32 indexCount, u32 indexSize, D3D11_PRIMITIVE_TOPOLOGY topology )
{
	mesh->m_vertexBuffer = nullptr;
	mesh->m_vertexCount = vertexCount;
	mesh->m_vertexSize = 0;
	mesh->m_vertexFormat = 0;
	if ( vertexCount > 0 && vertexSize > 0 && vertices != nullptr )
	{	
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = vertexSize * vertexCount;
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = vertices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufDesc, &data, &mesh->m_vertexBuffer ) );
		GPUResource_LogMemory( "VertexBuffer", bufDesc.ByteWidth, "mesh" );

		mesh->m_vertexSize = vertexSize;
		mesh->m_vertexFormat = vertexFormat;
	}
	
	mesh->m_indexBuffer = nullptr;
	mesh->m_indexCount = 0;
	mesh->m_indexSize = 0;
	if ( indexCount > 0 && indexSize > 0 && indices != nullptr )
	{
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = indexSize * indexCount;
		bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = indices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufDesc, &data, &mesh->m_indexBuffer ) );
		GPUResource_LogMemory( "IndexBuffer", bufDesc.ByteWidth, "mesh" );

		mesh->m_indexCount = indexCount;
		mesh->m_indexSize = indexSize;
	}

	mesh->m_topology = topology;
}

void GPUMesh_CreateTriangle( GPUMesh_s* mesh )
{
	const BasicVertex_s vertices[3] = 
	{
		{ Vec3_Make( 0.0f, 1.0f, 0.0f ), Color_Make( 255, 0, 0, 255) },
		{ Vec3_Make( 1.0f, -1.0f, 0.0f ), Color_Make( 0, 255, 0, 255) },
		{ Vec3_Make( -1.0f, -1.0f, 0.0f ), Color_Make( 0, 0, 255, 255) } 
	};

	const u16 indices[3] = { 0, 1, 2 };

	GPUMesh_Create( mesh, vertices, 3, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 3, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

void GPUMesh_CreateBox( GPUMesh_s* mesh, const Color_s color, bool wireframe )
{
	const BasicVertex_s vertices[8] = 
	{
		{ Vec3_Make( -1.0f, -1.0f,  1.0f ), color },
		{ Vec3_Make(  1.0f, -1.0f,  1.0f ), color },
		{ Vec3_Make( -1.0f,  1.0f,  1.0f ), color },
		{ Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
		{ Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ Vec3_Make(  1.0f, -1.0f, -1.0f ), color },
		{ Vec3_Make( -1.0f,  1.0f, -1.0f ), color },
		{ Vec3_Make(  1.0f,  1.0f, -1.0f ), color },
	};

	if ( wireframe )
	{
		const u16 indices[24] = { 
			0, 1, 1, 3, 3, 2, 2, 0,
			4, 5, 5, 7, 7, 6, 6, 4,
			1, 5, 0, 4, 3, 7, 2, 6 };

		GPUMesh_Create( mesh, vertices, 8, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 24, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
	}
	else
	{
		const u16 indices[36] = { 
			0, 2, 3,
			0, 3, 1,
			1, 3, 7, 
			1, 7, 5, 
			5, 7, 6,
			5, 6, 4,
			4, 6, 2,
			4, 2, 0, 
			2, 6, 7, 
			2, 7, 3,
			1, 5, 4,
			1, 4, 0 };

		GPUMesh_Create( mesh, vertices, 8, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 36, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	}
}

void GPUMesh_CreateSphere( GPUMesh_s* mesh, const Color_s color, u32 segmentCount, IStack* stack )
{
	ScopedStack scopedStack( stack );

	V6_ASSERT( segmentCount >= 2 );

	Vec2* circleCoords = stack->newArray< Vec2 >( segmentCount );
	
	const float phiStep = 2.0f * V6_PI / segmentCount;
	float phi = 0.0f;
	for ( u32 coordID = 0; coordID < segmentCount; ++coordID, phi += phiStep )
	{
		circleCoords[coordID].x = Cos( phi );
		circleCoords[coordID].y = Sin( phi );
	}

	const u32 circleCount = segmentCount-1;
	const u32 vertexCount = circleCount * segmentCount + 2;

	BasicVertex_s* vertices = stack->newArray< BasicVertex_s >( vertexCount );
	u32 vertexID = 0;

	vertices[vertexID].position = Vec3_Make( 0.0f, 0.0f, 1.0f );
	vertices[vertexID].color = color;
	++vertexID;

	const float thetaStep = V6_PI / (circleCount + 1);
	float theta = thetaStep;
	for ( u32 circleID = 0; circleID < circleCount; ++circleID, theta += thetaStep )
	{
		const float height = Cos( theta );
		const float radius = Sin( theta );
		for ( u32 coordID = 0; coordID < segmentCount; ++coordID )
		{
			vertices[vertexID].position = Vec3_Make( circleCoords[coordID].x * radius, circleCoords[coordID].y * radius, height );
			vertices[vertexID].color = color;
			++vertexID;
		}
	}

	vertices[vertexID].position = Vec3_Make( 0.0f, 0.0f, -1.0f );
	vertices[vertexID].color = color;
	++vertexID;

	V6_ASSERT( vertexID == vertexCount );

	const u32 indexCount = circleCount * segmentCount * 3 * 2;

	u16* indices = stack->newArray< u16 >( indexCount );
	u16 indexID = 0;

	for ( u32 coordID = 0; coordID < segmentCount; ++coordID )
	{
		indices[indexID+0] = 0;
		indices[indexID+1] = 1 + coordID;
		indices[indexID+2] = 1 + (coordID+1) % segmentCount;
		indexID += 3;
	}

	vertexID = 1;
	for ( u32 circleID = 0; circleID < circleCount-1; ++circleID )
	{
		for ( u32 coordID = 0; coordID < segmentCount; ++coordID )
		{
			indices[indexID+0] = vertexID + coordID;
			indices[indexID+1] = vertexID + segmentCount + coordID;
			indices[indexID+2] = vertexID + segmentCount + (coordID+1) % segmentCount;
			indexID += 3;

			indices[indexID+0] = vertexID + coordID;
			indices[indexID+1] = vertexID + segmentCount + (coordID+1) % segmentCount;
			indices[indexID+2] = vertexID + (coordID+1) % segmentCount;
			indexID += 3;
		}

		vertexID += segmentCount;
	}

	for ( u32 coordID = 0; coordID < segmentCount; ++coordID )
	{
		indices[indexID+0] = vertexID + coordID;
		indices[indexID+1] = vertexCount-1;
		indices[indexID+2] = vertexID + (coordID+1) % segmentCount;
		indexID += 3;
	}

	V6_ASSERT( indexID == indexCount );

	GPUMesh_Create( mesh, vertices, vertexCount, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, indexCount, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

void GPUMesh_CreateQuad( GPUMesh_s* mesh, const Color_s color )
{
	const BasicVertex_s vertices[8] = 
	{
		{ Vec3_Make( -1.0f, -1.0f, 0.0f ), color },
		{ Vec3_Make(  1.0f, -1.0f, 0.0f ), color },
		{ Vec3_Make( -1.0f,  1.0f, 0.0f ), color },
		{ Vec3_Make(  1.0f,  1.0f, 0.0f ), color },
	};

	const u16 indices[4] = { 0, 2, 1, 3 };

	GPUMesh_Create( mesh, vertices, 4, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 4, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

void GPUMesh_CreatePoint( GPUMesh_s* mesh )
{
	GPUMesh_Create( mesh, nullptr, 0, 0, 0, nullptr, 0, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
}

void GPUMesh_CreateLine( GPUMesh_s* mesh, const Color_s color )
{
	const BasicVertex_s vertices[2] = 
	{
		{ Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
	};

	GPUMesh_Create( mesh, vertices, 2, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, nullptr, 0, sizeof( u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
}

void GPUMesh_Release( GPUMesh_s* mesh )
{
	if ( mesh->m_vertexBuffer )
		V6_RELEASE_D3D11( mesh->m_vertexBuffer );
	if ( mesh->m_indexBuffer )
		V6_RELEASE_D3D11( mesh->m_indexBuffer );
}

void GPUMesh_DrawIndirect( GPUMesh_s* mesh, u32 instanceCount, GPUShader_s* shader, GPUBuffer_s* bufferArgs, u32 offsetArgs )
{
	V6_ASSERT( shader->m_vertexFormat == mesh->m_vertexFormat );
	V6_ASSERT( instanceCount > 0 );

	g_deviceContext->IASetInputLayout( shader->m_inputLayout );
	g_deviceContext->VSSetShader( shader->m_vertexShader, nullptr, 0 );
	g_deviceContext->PSSetShader( shader->m_pixelShader, nullptr, 0 );
		
	const u32 stride = mesh->m_vertexSize; 
	const u32 offset = 0;
			
	g_deviceContext->IASetVertexBuffers( 0, mesh->m_vertexBuffer != nullptr ? 1 : 0, &mesh->m_vertexBuffer, &stride, &offset );	
	g_deviceContext->IASetPrimitiveTopology( mesh->m_topology );

	if ( mesh->m_indexCount )
	{
		switch ( mesh->m_indexSize )
		{
		case 2:
			g_deviceContext->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R16_UINT, 0 );
			break;
		case 4:
			g_deviceContext->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
			break;
		default:
			V6_ASSERT_NOT_SUPPORTED();
		}

		if ( bufferArgs )
			g_deviceContext->DrawIndexedInstancedIndirect( bufferArgs->buf, offsetArgs );
		else if ( instanceCount == 1 )
			g_deviceContext->DrawIndexed( mesh->m_indexCount, 0, 0 );
		else
			g_deviceContext->DrawIndexedInstanced( mesh->m_indexCount, instanceCount, 0, 0, 0 );
	}
	else
	{
		V6_ASSERT( mesh->m_indexBuffer == nullptr );
		g_deviceContext->IASetIndexBuffer( nullptr, DXGI_FORMAT_R32_UINT, 0 );
				
		if ( bufferArgs )
			g_deviceContext->DrawInstancedIndirect( bufferArgs->buf, offsetArgs );
		else 
		{
			V6_ASSERT( mesh->m_vertexCount > 0 );
			if ( instanceCount == 1 )
				g_deviceContext->Draw( mesh->m_vertexCount, 0 );
			else
				g_deviceContext->DrawInstanced( mesh->m_vertexCount, instanceCount, 0, 0 );
		}
	}
}

void GPUMesh_Draw( GPUMesh_s* mesh, u32 instanceCount, GPUShader_s* shader )
{
	GPUMesh_DrawIndirect( mesh, instanceCount, shader, nullptr, 0 );
}

void GPURenderTargetState_Init( GPURenderTargetState_s* renderTargetState )
{	
	V6_ASSERT( GPURenderTargetState_s::COLOR_TARGET_COUNT == D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT );

	memset( renderTargetState, 0, sizeof( *renderTargetState ) );
}

void GPURenderTargetState_Save( GPURenderTargetState_s* renderTargetState )
{
	GPURenderTargetState_Init( renderTargetState );

	g_deviceContext->OMGetRenderTargets( GPURenderTargetState_s::COLOR_TARGET_COUNT, renderTargetState->rtvs, &renderTargetState->dsv );
}

void GPURenderTargetState_Restore( GPURenderTargetState_s* renderTargetState )
{
	g_deviceContext->OMSetRenderTargets( GPURenderTargetState_s::COLOR_TARGET_COUNT, renderTargetState->rtvs, renderTargetState->dsv );
}

void GPUShaderState_Init( GPUShaderState_s* shaderState )
{	
	V6_ASSERT( GPUShaderState_s::CB_SLOT_COUNT == D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT );
	V6_ASSERT( GPUShaderState_s::SRV_SLOT_COUNT == D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT );
	V6_ASSERT( GPUShaderState_s::UAV_SLOT_COUNT_D3D_11_0 == D3D11_PS_CS_UAV_REGISTER_COUNT );
	V6_ASSERT( GPUShaderState_s::UAV_SLOT_COUNT_D3D_11_1 == D3D11_1_UAV_SLOT_COUNT );

	memset( shaderState, 0, sizeof( *shaderState ) );
}

void GPUShaderState_Save( GPUShaderState_s* shaderState )
{
	GPUShaderState_Init( shaderState );

	g_deviceContext->CSGetShader( &shaderState->cs, nullptr, nullptr );
	g_deviceContext->CSGetConstantBuffers( 0, GPUShaderState_s::CB_SLOT_COUNT, shaderState->cbs );
	g_deviceContext->CSGetShaderResources( 0, GPUShaderState_s::SRV_SLOT_COUNT, shaderState->srvs );
	if ( g_device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0 )
		g_deviceContext->CSGetUnorderedAccessViews( 0, GPUShaderState_s::UAV_SLOT_COUNT_D3D_11_0, shaderState->uavs );
	else
		g_deviceContext->CSGetUnorderedAccessViews( 0, GPUShaderState_s::UAV_SLOT_COUNT_D3D_11_1, shaderState->uavs );
}

void GPUShaderState_Restore( GPUShaderState_s* shaderState )
{
	g_deviceContext->CSSetShader( shaderState->cs, nullptr, 0 );
	g_deviceContext->CSSetConstantBuffers( 0, GPUShaderState_s::CB_SLOT_COUNT, shaderState->cbs );
	g_deviceContext->CSSetShaderResources( 0, GPUShaderState_s::SRV_SLOT_COUNT, shaderState->srvs );
	if ( g_device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0 )
		g_deviceContext->CSSetUnorderedAccessViews( 0, GPUShaderState_s::UAV_SLOT_COUNT_D3D_11_0, shaderState->uavs, nullptr );
	else
		g_deviceContext->CSSetUnorderedAccessViews( 0, GPUShaderState_s::UAV_SLOT_COUNT_D3D_11_1, shaderState->uavs, nullptr );
}

void GPURenderTargetSet_Create( GPURenderTargetSet_s* renderTargetSet, const GPURenderTargetSetCreationDesc_s* desc )
{
	memset( renderTargetSet, 0, sizeof(*renderTargetSet) );

	GPUColorRenderTarget_Create( &renderTargetSet->colorBuffers[0], desc->width, desc->height, 1, desc->bindable, desc->writable, String_Format( desc->stereo ? "%sLeftColor" : "%sColor", desc->name ) );
	renderTargetSet->width = desc->width;
	renderTargetSet->height = desc->height;

	if ( desc->stereo )
	{
		GPUColorRenderTarget_Create( &renderTargetSet->colorBuffers[1], desc->width, desc->height, 1, desc->bindable, desc->writable, String_Format( "%sRightColor", desc->name ) );
		renderTargetSet->stereo = true;
	}

	GPUDepthRenderTarget_Create( &renderTargetSet->depthBuffer, desc->width, desc->height, 1, desc->bindable, String_Format( "%sDepth", desc->name ) );

	if ( desc->supportMSAA )
	{
		GPUColorRenderTarget_Create( &renderTargetSet->colorBufferMSAA, desc->width, desc->height, 8, false, false, String_Format( "%sColorMSAA", desc->name ) );
		GPUDepthRenderTarget_Create( &renderTargetSet->depthBufferMSAA, desc->width, desc->height, 8, false, String_Format( "%sDepthMSAA", desc->name ) );
		renderTargetSet->supportMSAA = true;
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

		V6_ASSERT_D3D11( g_device->CreateDepthStencilState( &depthStencilDesc, &renderTargetSet->depthStencilStateNoZ ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( g_device->CreateDepthStencilState( &depthStencilDesc, &renderTargetSet->depthStencilStateZRO ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( g_device->CreateDepthStencilState( &depthStencilDesc, &renderTargetSet->depthStencilStateZRW ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = false;
		blendState.RenderTarget[0].RenderTargetWriteMask = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBlendState( &blendState, &renderTargetSet->blendStateNoColor ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = false;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( g_device->CreateBlendState( &blendState, &renderTargetSet->blendStateOpaque ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = true;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = false;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( g_device->CreateBlendState( &blendState, &renderTargetSet->blendStateAlphaCoverage ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = true;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( g_device->CreateBlendState( &blendState, &renderTargetSet->blendStateAdditif ) );
	}

	{
		D3D11_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.DepthBias = 0;
		rasterDesc.DepthBiasClamp = 0;
		rasterDesc.SlopeScaledDepthBias = 0.0f;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.ScissorEnable = false;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.AntialiasedLineEnable = false;
		
		V6_ASSERT_D3D11( g_device->CreateRasterizerState( &rasterDesc, &renderTargetSet->rasterState ) );
	}
}

void GPURenderTargetSet_Bind( GPURenderTargetSet_s* renderTargetSet, const GPURenderTargetSetBindingDesc_s* desc, u32 eye )
{
	// Rasterization state
	g_deviceContext->OMSetDepthStencilState( desc->noZ ? renderTargetSet->depthStencilStateNoZ : renderTargetSet->depthStencilStateZRW, 0 );
	switch( desc->blendMode )
	{
	case GPU_BLEND_MODE_NO_COLOR:
		g_deviceContext->OMSetBlendState( renderTargetSet->blendStateNoColor, nullptr, 0XFFFFFFFF );
		break;
	case GPU_BLEND_MODE_OPAQUE:
		g_deviceContext->OMSetBlendState( renderTargetSet->blendStateOpaque, nullptr, 0XFFFFFFFF );
		break;
	case GPU_BLEND_MODE_ALPHA_COVERAGE:
		g_deviceContext->OMSetBlendState( renderTargetSet->blendStateAlphaCoverage, nullptr, 0XFFFFFFFF );
		break;
	case GPU_BLEND_MODE_ADDITIF:
		g_deviceContext->OMSetBlendState( renderTargetSet->blendStateAdditif, nullptr, 0XFFFFFFFF );
		break;
	default:
		V6_ASSERT_NOT_SUPPORTED();
	}
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)renderTargetSet->width;
		viewport.Height = (float)renderTargetSet->height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		g_deviceContext->RSSetViewports( 1, &viewport );
		g_deviceContext->RSSetState( renderTargetSet->rasterState );
	}
	
	// RT
	if ( desc->useMSAA )
	{
		V6_ASSERT( renderTargetSet->supportMSAA );
		g_deviceContext->OMSetRenderTargets( 1, &renderTargetSet->colorBufferMSAA.rtv, desc->noZ ? nullptr : renderTargetSet->depthBufferMSAA.dsv );
		renderTargetSet->bindingState.resolve = true;
	}
	else
	{
		g_deviceContext->OMSetRenderTargets( 1, &renderTargetSet->colorBuffers[eye].rtv, desc->noZ ? nullptr : renderTargetSet->depthBuffer.dsv );
		renderTargetSet->bindingState.eye = eye;
		renderTargetSet->bindingState.resolve = false;
	}
	renderTargetSet->bindingState.eye = eye;

	// Clear
	if ( desc->clear )
	{
		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		if ( desc->useMSAA )
		{
			g_deviceContext->ClearRenderTargetView( renderTargetSet->colorBufferMSAA.rtv, pRGBA );
			if ( !desc->noZ )
				g_deviceContext->ClearDepthStencilView( renderTargetSet->depthBufferMSAA.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
		}
		else
		{
			g_deviceContext->ClearRenderTargetView( renderTargetSet->colorBuffers[eye].rtv, pRGBA );
			if ( !desc->noZ )
				g_deviceContext->ClearDepthStencilView( renderTargetSet->depthBuffer.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
		}
	}
}

void GPURenderTargetSet_Unbind( GPURenderTargetSet_s* renderTargetSet )
{
	// un RT
	g_deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );

	if ( renderTargetSet->bindingState.resolve )
		g_deviceContext->ResolveSubresource( renderTargetSet->colorBuffers[renderTargetSet->bindingState.eye].tex, 0, renderTargetSet->colorBufferMSAA.tex, 0, DXGI_FORMAT_R8G8B8A8_UNORM );
}

void GPURenderTargetSet_Release( GPURenderTargetSet_s* renderTargetSet )
{
	GPUColorRenderTarget_Release( &renderTargetSet->colorBuffers[0] );
	if ( renderTargetSet->stereo )
		GPUColorRenderTarget_Release( &renderTargetSet->colorBuffers[1] );
	GPUDepthRenderTarget_Release( &renderTargetSet->depthBuffer );

	if ( renderTargetSet->supportMSAA )
	{
		GPUColorRenderTarget_Release( &renderTargetSet->colorBufferMSAA );
		GPUDepthRenderTarget_Release( &renderTargetSet->depthBufferMSAA );
	}

	V6_RELEASE_D3D11( renderTargetSet->depthStencilStateNoZ );
	V6_RELEASE_D3D11( renderTargetSet->depthStencilStateZRO );
	V6_RELEASE_D3D11( renderTargetSet->depthStencilStateZRW );
	V6_RELEASE_D3D11( renderTargetSet->blendStateNoColor );
	V6_RELEASE_D3D11( renderTargetSet->blendStateOpaque );
	V6_RELEASE_D3D11( renderTargetSet->blendStateAlphaCoverage );
	V6_RELEASE_D3D11( renderTargetSet->blendStateAdditif );
	V6_RELEASE_D3D11( renderTargetSet->rasterState );
}

void GPUShaderContext_CreateEmpty()
{
	V6_ASSERT( !s_shaderContext.initialized );

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = D3D11_REQ_MAXANISOTROPY;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		V6_ASSERT_D3D11( g_device->CreateSamplerState( &samplerDesc, &s_shaderContext.trilinearSamplerState ) );
	}

	s_shaderContext.initialized = true;
}

void GPUShaderContext_Release()
{
	V6_ASSERT( s_shaderContext.initialized );

	for ( u32 constantBufferID = 0; constantBufferID < GPUShaderContext_s::CONSTANT_BUFFER_MAX_COUNT; ++constantBufferID )
	{
		if ( s_shaderContext.constantBuffers[constantBufferID ].buf )
			GPUConstantBuffer_Release( &s_shaderContext.constantBuffers[constantBufferID ] );
	}

	for ( u32 computeID = 0; computeID < GPUShaderContext_s::COMPUTE_MAX_COUNT; ++computeID )
		if ( s_shaderContext.computes[computeID].m_computeShader )
			GPUCompute_Release( &s_shaderContext.computes[computeID] );

	for ( u32 shaderID = 0; shaderID < GPUShaderContext_s::SHADER_MAX_COUNT; ++shaderID )
		if ( s_shaderContext.shaders[shaderID].m_vertexShader )
			GPUShader_Release( &s_shaderContext.shaders[shaderID] );
	
	for ( u32 bufferID = 0; bufferID < GPUShaderContext_s::BUFFER_MAX_COUNT; ++bufferID )
		if ( s_shaderContext.buffers[bufferID].buf )
			GPUBuffer_Release( &s_shaderContext.buffers[bufferID] );

	V6_RELEASE_D3D11( s_shaderContext.trilinearSamplerState );

	memset( &s_shaderContext, 0, sizeof( GPUShaderContext_s ) );
}

GPUShaderContext_s* GPUShaderContext_Get()
{
	V6_ASSERT( s_shaderContext.initialized );
	return &s_shaderContext;
}

GPUSurfaceContext_s* GPUSurfaceContext_Get()
{
	V6_ASSERT( s_surfaceContext.initialized );
	return &s_surfaceContext;
}

void GPUSurfaceContext_Present()
{
	V6_ASSERT( s_surfaceContext.initialized );
	s_surfaceContext.swapChain->Present( 0, 0 );
}

ID3D11Device* GPUDevice_Get()
{
	V6_ASSERT( g_device != nullptr );
	return g_device;
}

void GPUDevice_Set( ID3D11Device* device )
{
	V6_ASSERT( device );
	V6_ASSERT( g_device == nullptr );

	g_device = device;
	g_device->AddRef();
	g_device->GetImmediateContext( &g_deviceContext );
	V6_ASSERT_D3D11( g_deviceContext->QueryInterface( IID_PPV_ARGS( &s_userDefinedAnnotation ) ) );
}

void GPUDevice_CreateWithSurfaceContext( u32 width, u32 height, void* hWnd, bool debug )
{
	V6_ASSERT( g_device == nullptr );

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.OutputWindow = (HWND)hWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = 0;

	D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_1 };
	
	D3D_FEATURE_LEVEL featureLevel;
	V6_ASSERT_D3D11( D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		(debug ? D3D11_CREATE_DEVICE_DEBUG : 0) | D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT,
		pFeatureLevels,
		1,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&s_surfaceContext.swapChain,
		&g_device,
		&featureLevel,
		&g_deviceContext ) );

	V6_ASSERT( featureLevel == D3D_FEATURE_LEVEL_11_1 );
	
	V6_ASSERT_D3D11( g_deviceContext->QueryInterface( IID_PPV_ARGS( &s_userDefinedAnnotation ) ) );

#if 0
	for ( u32 sampleCount = 1; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount++)
	{
		u32 maxQualityLevel;
		HRESULT hr = m_device->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, MSAA_SAMPLE_QUALITY, &maxQualityLevel );
		
		if ( hr != S_OK )
			break;
		
		if ( maxQualityLevel > 0 )
			V6_MSG ("MSAA %dX supported with %d quality levels.\n", sampleCount, maxQualityLevel-1 );
	}
#endif

	V6_ASSERT_D3D11( s_surfaceContext.swapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&s_surfaceContext.surface.tex ) );

	V6_ASSERT_D3D11( g_device->CreateRenderTargetView( s_surfaceContext.surface.tex, 0, &s_surfaceContext.surface.rtv ) );
	V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( s_surfaceContext.surface.tex, 0, &s_surfaceContext.surface.uav ) );
	s_surfaceContext.initialized = true;
}

void GPUDevice_Release()
{
	V6_ASSERT( g_device );

	if ( s_surfaceContext.initialized )
	{
		g_deviceContext->ClearState();

		V6_RELEASE_D3D11( s_surfaceContext.surface.tex );
		V6_RELEASE_D3D11( s_surfaceContext.surface.rtv );
		V6_RELEASE_D3D11( s_surfaceContext.surface.uav );
		V6_RELEASE_D3D11( s_surfaceContext.swapChain );
		memset( &s_surfaceContext, 0, sizeof( s_surfaceContext ) );
	}

	if ( s_surfaceContext.initialized )
		GPUShaderContext_Release();

	GPUEventContext_Release();

	V6_RELEASE_D3D11( s_userDefinedAnnotation );
	V6_RELEASE_D3D11( g_deviceContext );
	V6_RELEASE_D3D11( g_device );
}

END_V6_NAMESPACE
