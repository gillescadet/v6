/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_GPU_H__
#define __V6_GRAPHIC_GPU_H__

#include <v6/core/color.h>
#include <v6/core/mat4x4.h>
#include <v6/core/vec2.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3i.h>

#define V6_ASSERT_D3D11( EXP )	{ HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }
#define V6_RELEASE_D3D11( EXP )	{ V6_ASSERT( EXP ); EXP->Release(); EXP = nullptr; }

BEGIN_V6_NAMESPACE

class IAllocator;

enum
{
	VERTEX_FORMAT_POSITION		= 1 << 0,
	
	VERTEX_FORMAT_COLOR			= 1 << 1,
		
	VERTEX_FORMAT_USER0_SHIFT	= 2,
	VERTEX_FORMAT_USER0_MASK	= 7 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F1		= 1 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F2		= 2 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F3		= 3 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F4		= 4 << VERTEX_FORMAT_USER0_SHIFT,
	
	
	VERTEX_FORMAT_USER1_SHIFT	= 5,
	VERTEX_FORMAT_USER1_MASK	= 7 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F1		= 1 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F2		= 2 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F3		= 3 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F4		= 4 << VERTEX_FORMAT_USER1_SHIFT,

	VERTEX_FORMAT_USER2_SHIFT	= 8,
	VERTEX_FORMAT_USER2_MASK	= 7 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F1		= 1 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F2		= 2 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F3		= 3 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F4		= 4 << VERTEX_FORMAT_USER2_SHIFT,

	VERTEX_FORMAT_USER3_SHIFT	= 11,
	VERTEX_FORMAT_USER3_MASK	= 7 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F1		= 1 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F2		= 2 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F3		= 3 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F4		= 4 << VERTEX_FORMAT_USER3_SHIFT,
};

enum GPUBufferCreationFlag_e
{
	GPUBUFFER_CREATION_FLAG_READ_BACK	= 1 << 0,
	GPUBUFFER_CREATION_FLAG_DYNAMIC		= 1 << 1,
};

struct GPUBuffer_s
{
	ID3D11Buffer*					buf;
	ID3D11Buffer*					staging;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
};

enum GPUTextureMipMapState_e
{
	GPUTEXTURE_MIPMAP_STATE_NONE,
	GPUTEXTURE_MIPMAP_STATE_REQUIRED,
	GPUTEXTURE_MIPMAP_STATE_GENERATED,
};

struct GPUTexture2D_s
{
	ID3D11Texture2D*				tex;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
	GPUTextureMipMapState_e			mipmapState;
};

struct GPUColorRenderTarget_s
{
	ID3D11Texture2D*				tex;
	ID3D11RenderTargetView*			rtv;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
};

struct GPUDepthRenderTarget_s
{
	ID3D11Texture2D*				tex;
	ID3D11DepthStencilView*			dsv;
	ID3D11ShaderResourceView*		srv;
};

struct GPUConstantBuffer_s
{
	ID3D11Buffer*					buf;
};

struct GPUCompute_s
{
	ID3D11ComputeShader*			m_computeShader;
};

struct GPUShader_s
{
	ID3D11VertexShader*				m_vertexShader;
	ID3D11PixelShader*				m_pixelShader;

	ID3D11InputLayout*				m_inputLayout;

	u32								m_vertexFormat;
};

struct GPUMesh_s
{
	ID3D11Buffer*					m_vertexBuffer;
	ID3D11Buffer*					m_indexBuffer;
	u32								m_vertexCount;
	u32								m_vertexSize;
	u32								m_vertexFormat;
	u32								m_indexCount;
	u32								m_indexSize;
	D3D11_PRIMITIVE_TOPOLOGY		m_topology;	
};

struct GPUQuery_s 
{
	ID3D11Query*					query;
	u64								data;
};

struct GPURenderTargetState_s
{
	static const u32				COLOR_TARGET_COUNT = 8;

	ID3D11RenderTargetView*			rtvs[COLOR_TARGET_COUNT];
	ID3D11DepthStencilView*			dsv;
};

struct GPUShaderState_s
{
	static const u32				CB_SLOT_COUNT = 14;
	static const u32				SRV_SLOT_COUNT = 128;
	static const u32				UAV_SLOT_COUNT_D3D_11_0 = 8;
	static const u32				UAV_SLOT_COUNT_D3D_11_1 = 64;
	
	// CS
	ID3D11ComputeShader*			cs;
	ID3D11Buffer*					cbs[CB_SLOT_COUNT];
	ID3D11ShaderResourceView*		srvs[SRV_SLOT_COUNT];
	ID3D11UnorderedAccessView*		uavs[UAV_SLOT_COUNT_D3D_11_1];
};

struct GPUSurfaceContext_s
{
	GPUColorRenderTarget_s			surface;

	IDXGISwapChain*					swapChain;

	bool							initialized;
};

struct GPURenderTargetSetCreationDesc_s
{
	const char*						name;
	u32								width;
	u32								height;
	bool							supportMSAA;
	bool							bindable;
	bool							writable;
	bool							stereo;
};

struct GPURenderTargetSetBindingDesc_s
{
	bool							clear;
	bool							noZ;
	bool							useMSAA;
	bool							useAlphaCoverage;
};

struct GPURenderTargetSet_s
{
	GPUColorRenderTarget_s			colorBuffers[2];
	GPUDepthRenderTarget_s			depthBuffer;
	GPUColorRenderTarget_s			colorBufferMSAA;
	GPUDepthRenderTarget_s			depthBufferMSAA;

	ID3D11DepthStencilState*		depthStencilStateNoZ;
	ID3D11DepthStencilState*		depthStencilStateZRO;
	ID3D11DepthStencilState*		depthStencilStateZRW;
	ID3D11BlendState*				blendStateNoColor;
	ID3D11BlendState*				blendStateOpaque;
	ID3D11BlendState*				blendStateAlphaCoverage;
	ID3D11BlendState*				blendStateAdditif;
	ID3D11RasterizerState*			rasterState;

	u32								width;
	u32								height;
	bool							supportMSAA;
	bool							stereo;

	struct
	{
		u32							eye;
		bool						resolve;
	} bindingState;
};

struct GPUShaderContext_s
{
	static const u32				CONSTANT_BUFFER_MAX_COUNT = 64;
	static const u32				COMPUTE_MAX_COUNT = 64;
	static const u32				SHADER_MAX_COUNT = 64;

	GPUConstantBuffer_s				constantBuffers[CONSTANT_BUFFER_MAX_COUNT];
	GPUCompute_s					computes[COMPUTE_MAX_COUNT];
	GPUShader_s						shaders[SHADER_MAX_COUNT];

	ID3D11SamplerState*				samplerState;

	bool							initialized;
};

struct GPUQueryContext_s
{	
	static const u32				QUERY_MAX_COUNT = 64;

	GPUQuery_s						queries[2][QUERY_MAX_COUNT];

	bool							initialized;
};

void						GPUBuffer_CreateIndirectArgs( GPUBuffer_s* buffer, u32 count, u32 flags, const char* name );
void						GPUBuffer_CreateIndirectArgsWithStaticData( GPUBuffer_s* buffer, const void* data, u32 count, u32 flags, const char* name );
void						GPUBuffer_CreateStructured( GPUBuffer_s* buffer, u32 elementSize, u32 count, u32 flags, const char* name );
void						GPUBuffer_CreateStructuredWithStaticData( GPUBuffer_s* buffer, const void* data, u32 elementSize, u32 count, u32 flags, const char* name );
void						GPUBuffer_CreateTyped( GPUBuffer_s* buffer, DXGI_FORMAT format, u32 count, u32 flags, const char* name );
const void*					GPUBuffer_MapReadBack( GPUBuffer_s* buffer );
void						GPUBuffer_UnmapReadBack( GPUBuffer_s* buffer );
void						GPUBuffer_Release( GPUBuffer_s* buffer );
void						GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const void* srcData, u32 sizeOfSrcElem, u32 srcCount );
template < typename T >
void						GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const T* srcData, u32 srcCount );

void						GPUDevice_CreateWithSurfaceContext( u32 width, u32 height, void* hWnd, bool debug );
void						GPUDevice_Set( ID3D11Device* device );
void						GPUDevice_Release();

void						GPUCompute_CreateFromSource( GPUCompute_s* compute, const void* source, u32 sourceSize );
bool						GPUCompute_CreateFromFile( GPUCompute_s* compute, const char* cs, IAllocator* allocator );
void						GPUCompute_Dispatch( GPUCompute_s* compute, u32 groupX, u32 groupY, u32 groupZ );
void						GPUCompute_DispatchIndirect( GPUCompute_s* compute, GPUBuffer_s* bufferArgs, u32 offsetArgs );
void						GPUCompute_Release( GPUCompute_s* compute );

void						GPUConstantBuffer_Create( GPUConstantBuffer_s* buffer, u32 sizeOfStruct, const char* name );
void*						GPUConstantBuffer_MapWrite( GPUConstantBuffer_s* buffer );
void						GPUConstantBuffer_Release( GPUConstantBuffer_s* buffer );
void						GPUConstantBuffer_UnmapWrite( GPUConstantBuffer_s* buffer );

void						GPUColorRenderTarget_Copy( GPUColorRenderTarget_s* dstColorRenderTarget, GPUColorRenderTarget_s* srcColorRenderTarget  );
void						GPUColorRenderTarget_Create( GPUColorRenderTarget_s* colorRenderTarget, u32 width, u32 height, u32 sampleCount, bool bindable, bool writable, const char* name );
void						GPUColorRenderTarget_Release( GPUColorRenderTarget_s* colorRenderTarget );
void						GPUDepthRenderTarget_Create( GPUDepthRenderTarget_s* colorRenderTarget, u32 width, u32 height, u32 sampleCount, bool bindable, const char* name );
void						GPUDepthRenderTarget_Release( GPUDepthRenderTarget_s* depthRenderTarget );

void						GPUEvent_Begin( const char* eventName );
void						GPUEvent_BeginW( const wchar_t* eventNameW );
void						GPUEvent_End();

void						GPUMesh_Create( GPUMesh_s* mesh, const void* vertices, u32 vertexCount, u32 vertexSize, u32 vertexFormat, const void* indices, u32 indexCount, u32 indexSize, D3D11_PRIMITIVE_TOPOLOGY topology );
void						GPUMesh_CreateTriangle( GPUMesh_s* mesh );
void						GPUMesh_CreateBox( GPUMesh_s* mesh, const Color_s color, bool wireframe );
void						GPUMesh_CreateQuad( GPUMesh_s* mesh, const Color_s color );
void						GPUMesh_CreatePoint( GPUMesh_s* mesh );
void						GPUMesh_CreateLine( GPUMesh_s* mesh, const Color_s color );
void						GPUMesh_Draw( GPUMesh_s* mesh, u32 instanceCount, GPUShader_s* shader );
void						GPUMesh_DrawIndirect( GPUMesh_s* mesh, u32 instanceCount, GPUShader_s* shader, GPUBuffer_s* bufferArgs, u32 offsetArgs );
void						GPUMesh_Release( GPUMesh_s* mesh );
void						GPUMesh_UpdateVertices( GPUMesh_s* mesh, const void* vertices );

void						GPUQuery_BeginTimeStampDisjoint( GPUQuery_s* query );
void						GPUQuery_CreateTimeStamp( GPUQuery_s* query );
void						GPUQuery_CreateTimeStampDisjoint( GPUQuery_s* query );
void						GPUQuery_EndTimeStampDisjoint( GPUQuery_s* query );
float						GPUQuery_GetElpasedTime( const GPUQuery_s* queryStart, const GPUQuery_s* queryEnd, const GPUQuery_s* queryDisjoint );
bool						GPUQuery_ReadTimeStamp( GPUQuery_s* query );
bool						GPUQuery_ReadTimeStampDisjoint( GPUQuery_s* query );
void						GPUQuery_Release( GPUQuery_s* query );
void						GPUQuery_WriteTimeStamp( GPUQuery_s* query );

void						GPUQueryContext_CreateEmpty();
GPUQueryContext_s*			GPUQueryContext_Get();
void						GPUQueryContext_Release();

void						GPURenderTargetSet_Bind( GPURenderTargetSet_s* renderTargetSet, const GPURenderTargetSetBindingDesc_s* desc, u32 eye );
void						GPURenderTargetSet_Create( GPURenderTargetSet_s* renderTargetSet, const GPURenderTargetSetCreationDesc_s* desc );
void						GPURenderTargetSet_Release( GPURenderTargetSet_s* renderTargetSet );
void						GPURenderTargetSet_Unbind( GPURenderTargetSet_s* renderTargetSet );

void						GPUResource_LogMemory( const char* res, u32 size, const char* name );
void						GPUResource_LogMemoryUsage();

bool						GPUShader_Create( GPUShader_s* shader, const char* vs, const char* ps, u32 vertexFormat, IAllocator* allocator );
void						GPUShader_Release( GPUShader_s* shader );

void						GPUShaderContext_CreateEmpty();
GPUShaderContext_s*			GPUShaderContext_Get();
void						GPUShaderContext_Release();

GPUSurfaceContext_s*		GPUSurfaceContext_Get();
void						GPUSurfaceContext_Present();

void						GPUTexture2D_Create( GPUTexture2D_s* tex, u32 width, u32 height, Color_s* pixels, bool mipmap, const char* name );
void						GPUTexture2D_CreateRW( GPUTexture2D_s* tex, u32 width, u32 height, const char* name );
void						GPUTexture2D_Release( GPUTexture2D_s* tex );

void						GPURenderTargetState_Init( GPURenderTargetState_s* renderTargetState );
void						GPURenderTargetState_Save( GPURenderTargetState_s* renderTargetState );
void						GPURenderTargetState_Restore( GPURenderTargetState_s* renderTargetState );

void						GPUShaderState_Init( GPUShaderState_s* shaderState );
void						GPUShaderState_Save( GPUShaderState_s* shaderState );
void						GPUShaderState_Restore( GPUShaderState_s* shaderState );

// inline

template < typename T >
void GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const T* srcData, u32 srcCount )
{
	GPUBuffer_Update( dstBuffer, dstOffset, srcData, sizeof( T ), srcCount );
}

inline void ReadBack_Log( const char* res, u32 value, const char* name )
{
	V6_MSG( "%-16s %-30s: %10d\n", res, name, value );
}

inline void ReadBack_Log( const char* res, hex32 value, const char* name )
{
	V6_MSG( "%-16s %-30s: 0x%08X\n", res, name, value.n );
}

inline void ReadBack_Log( const char* res, float value, const char* name )
{
	V6_MSG( "%-16s %-30s: %g\n", res, name, value );
}

inline void ReadBack_Log( const char* res, Vec2 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g)\n", res, name, value.x, value.y );
}

inline void ReadBack_Log( const char* res, Vec3 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g, %g)\n", res, name, value.x, value.y, value.z );
}

inline void ReadBack_Log( const char* res, Vec4 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g, %g, %g)\n", res, name, value.x, value.y, value.z, value.w );
}

inline void ReadBack_Log( const char* res, Vec2u value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4u, %4u)\n", res, name, value.x, value.y );
}

inline void ReadBack_Log( const char* res, Vec3u value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4u, %4u, %4u)\n", res, name, value.x, value.y, value.z );
}

inline void ReadBack_Log( const char* res, Vec2i value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4d, %4d)\n", res, name, value.x, value.y );
}

inline void ReadBack_Log( const char* res, Vec3i value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4d, %4d, %4d)\n", res, name, value.x, value.y, value.z );
}

inline void ReadBack_Log( const char* res, Mat4x4 value, const char* name )
{
	V6_MSG( "%-16s %-30s:\n", res, name );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row0.x, value.m_row0.y, value.m_row0.z, value.m_row0.w );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row1.x, value.m_row1.y, value.m_row1.z, value.m_row1.w );	
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row2.x, value.m_row2.y, value.m_row2.z, value.m_row2.w );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row3.x, value.m_row3.y, value.m_row3.z, value.m_row3.w );
}

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_GPU_H__
