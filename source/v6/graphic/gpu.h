/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_GPU_H__
#define __V6_GRAPHIC_GPU_H__

#include <v6/core/color.h>

#define V6_GPU_PROFILING 1

#define V6_ASSERT_D3D11( EXP )	{ HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }
#define V6_RELEASE_D3D11( EXP )	{ V6_ASSERT( EXP ); EXP->Release(); EXP = nullptr; }

BEGIN_V6_NAMESPACE

class CFileSystem;

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
#if V6_GPU_PROFILING == 1
	ID3D11Query*					query;
#endif
	u64								data;
};

void			GPU_SetDevice( ID3D11Device* device );

void			GPU_BeginEvent( const char* eventName );
void			GPU_EndEvent();

void			GPUBuffer_CreateIndirectArgs( GPUBuffer_s* buffer, u32 count, u32 flags, const char* name );
void			GPUBuffer_CreateIndirectArgsWithStaticData( GPUBuffer_s* buffer, const void* data, u32 count, u32 flags, const char* name );
void			GPUBuffer_CreateStructured( GPUBuffer_s* buffer, u32 elementSize, u32 count, u32 flags, const char* name );
void			GPUBuffer_CreateStructuredWithStaticData( GPUBuffer_s* buffer, const void* data, u32 elementSize, u32 count, u32 flags, const char* name );
void			GPUBuffer_CreateTyped( GPUBuffer_s* buffer, DXGI_FORMAT format, u32 count, u32 flags, const char* name );
const void*		GPUBuffer_MapReadBack( GPUBuffer_s* buffer );
void			GPUBuffer_UnmapReadBack( GPUBuffer_s* buffer );
void			GPUBuffer_Release( GPUBuffer_s* buffer );
void			GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const void* srcData, u32 sizeOfSrcElem, u32 srcCount );
template < typename T >
void			GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const T* srcData, u32 srcCount );

void			GPUCompute_CreateFromSource( GPUCompute_s* compute, const void* source, u32 sourceSize );
bool			GPUCompute_CreateFromFile( GPUCompute_s* compute, const char* cs, CFileSystem* fileSystem, IAllocator* allocator );
void			GPUCompute_Release( GPUCompute_s* compute );

void			GPUConstantBuffer_Create( GPUConstantBuffer_s* buffer, u32 sizeOfStruct, const char* name );
void*			GPUConstantBuffer_MapWrite( GPUConstantBuffer_s* buffer );
void			GPUConstantBuffer_Release( GPUConstantBuffer_s* buffer );
void			GPUConstantBuffer_UnmapWrite( GPUConstantBuffer_s* buffer );

void			GPUColorRenderTarget_Create( GPUColorRenderTarget_s* colorRenderTarget, u32 width, u32 height, u32 sampleCount, bool bindable, bool writable, const char* name );
void			GPUColorRenderTarget_Release( GPUColorRenderTarget_s* colorRenderTarget );
void			GPUDepthRenderTarget_Create( GPUDepthRenderTarget_s* colorRenderTarget, u32 width, u32 height, u32 sampleCount, bool bindable, const char* name );
void			GPUDepthRenderTarget_Release( GPUDepthRenderTarget_s* depthRenderTarget );

void			GPUMesh_Create( GPUMesh_s* mesh, const void* vertices, u32 vertexCount, u32 vertexSize, u32 vertexFormat, const void* indices, u32 indexCount, u32 indexSize, D3D11_PRIMITIVE_TOPOLOGY topology );
void			GPUMesh_Release( GPUMesh_s* mesh );
void			GPUMesh_UpdateVertices( GPUMesh_s* mesh, const void* vertices );

void			GPUQuery_BeginTimeStampDisjoint( GPUQuery_s* query );
void			GPUQuery_CreateTimeStamp( GPUQuery_s* query );
void			GPUQuery_CreateTimeStampDisjoint( GPUQuery_s* query );
void			GPUQuery_EndTimeStampDisjoint( GPUQuery_s* query );
float			GPUQuery_GetElpasedTime( const GPUQuery_s* queryStart, const GPUQuery_s* queryEnd, const GPUQuery_s* queryDisjoint );
bool			GPUQuery_ReadTimeStamp( GPUQuery_s* query );
bool			GPUQuery_ReadTimeStampDisjoint( GPUQuery_s* query );
void			GPUQuery_Release( GPUQuery_s* query );
void			GPUQuery_WriteTimeStamp( GPUQuery_s* query );

void			GPUResource_LogMemory( const char* res, u32 size, const char* name );
void			GPUResource_LogMemoryUsage();

bool			GPUShader_Create( GPUShader_s* shader, const char* vs, const char* ps, u32 vertexFormat, CFileSystem* fileSystem, IAllocator* allocator );
void			GPUShader_Release( GPUShader_s* shader );

void			GPUTexture2D_Create( GPUTexture2D_s* tex, u32 width, u32 height, Color_s* pixels, bool mipmap, const char* name );
void			GPUTexture2D_CreateRW( GPUTexture2D_s* tex, u32 width, u32 height, const char* name );
void			GPUTexture2D_Release( GPUTexture2D_s* tex );

// inline

template < typename T >
void GPUBuffer_Update( GPUBuffer_s* dstBuffer, u32 dstOffset, const T* srcData, u32 srcCount )
{
	GPUBuffer_Update( dstBuffer, dstOffset, srcData, sizeof( T ), srcCount );
}

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_GPU_H__
