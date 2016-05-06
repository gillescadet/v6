/*V6*/

#pragma warning( push, 3 )
#include <d3d11_1.h>
#pragma warning( pop )

#include <v6/core/common.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/string.h>
#include <v6/core/thread.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

static const u32 VERTEX_INPUT_MAX_COUNT = 6;

ID3D11Device*						g_device = nullptr;
ID3D11DeviceContext*				g_deviceContext = nullptr;
static ID3DUserDefinedAnnotation*	s_userDefinedAnnotation = nullptr;

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

void GPU_SetDevice( ID3D11Device* device )
{
	g_device = device;
	g_device->GetImmediateContext( &g_deviceContext );
	V6_ASSERT_D3D11( g_deviceContext->QueryInterface( IID_PPV_ARGS( &s_userDefinedAnnotation ) ) );
}

void GPU_BeginEvent( const char* eventName )
{
	const u32 maxLen = 256;
	const u32 len = (u32)strlen( eventName );
	V6_ASSERT( len < maxLen );
	WCHAR eventNameW[maxLen + 1];
	MultiByteToWideChar( CP_ACP, 0, eventName, len, eventNameW, maxLen );
	eventNameW[len] = 0;

	s_userDefinedAnnotation->BeginEvent( eventNameW );
}

void GPU_EndEvent()
{
	s_userDefinedAnnotation->EndEvent();
}

void GPUResource_LogMemory( const char* res, u32 size, const char* name )
{
	if ( DivMB( size ) >= 1 )
		V6_MSG( "%-16s %-30s: %8s MB\n", res, name, String_FormatInteger_Unsafe( DivMB( size ) ) );
	Atomic_Add( &s_gpuMemory, size );
}

void GPUResource_LogMemoryUsage()
{
	V6_MSG( "%-16s %-30s: %8s MB\n", "GPU", "total", String_FormatInteger_Unsafe( DivMB( s_gpuMemory ) ) );
}

void GPUCompute_CreateFromSource( GPUCompute_s* compute, const void* source, u32 sourceSize )
{
	V6_ASSERT_D3D11( g_device->CreateComputeShader( source, sourceSize, nullptr, &compute->m_computeShader ) );
}

bool GPUCompute_CreateFromFile( GPUCompute_s* compute, const char* cs, CFileSystem* fileSystem, IAllocator* allocator )
{
	Stack stack( allocator, 64 * 1024 );

	void* csBytecode = nullptr;
	const int csBytecodeSize = fileSystem->ReadFile( cs, &csBytecode, &stack );
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

bool GPUShader_Create( GPUShader_s* shader, const char* vs, const char* ps, u32 vertexFormat, CFileSystem* fileSystem, IAllocator* allocator )
{
	Stack stack( allocator, 64 * 1024 );

	void* vsBytecode = nullptr;
	const int vsBytecodeSize = fileSystem->ReadFile( vs, &vsBytecode, &stack );
	if ( vsBytecodeSize <= 0 )
	{
		V6_ERROR( "Unable to read file %s!\n", vs );
		return false;
	}

	{
		HRESULT hRes = g_device->CreateVertexShader( vsBytecode, vsBytecodeSize, nullptr, &shader->m_vertexShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!\n" );
			return false;
		}
	}

	void* psBytecode = nullptr;
	const int psBytecodeSize = fileSystem->ReadFile( ps, &psBytecode, &stack );
	if ( psBytecodeSize <= 0 )
	{
		V6_ERROR( "Unable to read file %s!\n", ps );
		return false;
	}

	{
		HRESULT hRes = g_device->CreatePixelShader( psBytecode, psBytecodeSize, nullptr, &shader->m_pixelShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreatePixelShader failed!\n" );
			return false;
		}
	}
	
	{
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

		HRESULT hRes = g_device->CreateInputLayout( idesc, inputCount, vsBytecode, vsBytecodeSize, &shader->m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!\n" );
			return false;
		}

		shader->m_vertexFormat = vertexFormat;
	}

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

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( u32 );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( u32 );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
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

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( u32 );
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
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
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

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
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

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
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
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
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

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.CPUAccessFlags = (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) != 0 ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}
	
	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( g_device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_DYNAMIC) == 0 )
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

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
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
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;

		V6_ASSERT_D3D11( g_device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
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
#if 0
	D3D11_BOX dstBox;
	dstBox.left = dstOffset * sizeof( T );
	dstBox.right = (dstOffset + srcCount) * sizeof( T );
	dstBox.front = 0;
	dstBox.back = 1;
	dstBox.top = 0;
	dstBox.bottom = 1;

	const u32 size = dstBox.right - dstBox.left;
	g_deviceContext->UpdateSubresource( dstBuffer->buf, 0, &dstBox, srcData, size, size );
#else
	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( g_deviceContext->Map( dstBuffer->buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &res ) );
	memcpy( (u8*)res.pData + dstOffset * sizeOfSrcElem, srcData, srcCount * sizeOfSrcElem );
	g_deviceContext->Unmap( dstBuffer->buf, 0 );
#endif
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
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "cubeDepth" );
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
		void* mipPixels = nullptr;
		
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

		if ( pixelCount > width * height )
		
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

#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( g_device->CreateQuery( &queryDesc, &query->query ) );
#endif
}

void GPUQuery_CreateTimeStampDisjoint( GPUQuery_s* query )
{
	query->data = 0;

#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( g_device->CreateQuery( &queryDesc, &query->query ) );
#endif
}

void GPUQuery_BeginTimeStampDisjoint( GPUQuery_s* query )
{
	V6_ASSERT( query->data != (u64)-1 );
	query->data = (u64)-1;
#if V6_GPU_PROFILING == 1
	g_deviceContext->Begin( query->query );
#endif
}

void GPUQuery_EndTimeStampDisjoint( GPUQuery_s* query )
{
	V6_ASSERT( query->data == (u64)-1 );
	query->data = 0;
#if V6_GPU_PROFILING == 1
	g_deviceContext->End( query->query );
#endif
}

bool GPUQuery_ReadTimeStampDisjoint( GPUQuery_s* query )
{
	V6_ASSERT( query->data != (u64)-1 );
#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint = {};
	if ( g_deviceContext->GetData( query->query, &timestampDisjoint, sizeof( timestampDisjoint ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) != S_OK )
		return false;
	if ( timestampDisjoint.Disjoint || timestampDisjoint.Frequency == 0 )
		return false;
	query->data = timestampDisjoint.Frequency;
	return true;
#else
	query->data = 0;
	return false;
#endif
	
}

void GPUQuery_WriteTimeStamp( GPUQuery_s* query )
{
	query->data = 0;
#if V6_GPU_PROFILING == 1
	g_deviceContext->End( query->query );
#endif
}

bool GPUQuery_ReadTimeStamp( GPUQuery_s* query )
{
#if V6_GPU_PROFILING == 1
	return g_deviceContext->GetData( query->query, &query->data, sizeof( query->data ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK;
#else
	return false;
#endif
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
#if V6_GPU_PROFILING == 1
	V6_RELEASE_D3D11( query->query );
#endif
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

void GPUMesh_Release( GPUMesh_s* mesh )
{
	if ( mesh->m_vertexBuffer )
		V6_RELEASE_D3D11( mesh->m_vertexBuffer );
	if ( mesh->m_indexBuffer )
		V6_RELEASE_D3D11( mesh->m_indexBuffer );
}

END_V6_NAMESPACE
