/*V6*/

#include <v6/core/common.h>
#include <v6/core/frame_manager.h>

#include <v6/core/memory.h>

BEGIN_V6_CORE_NAMESPACE

FrameManager::FrameManager(IHeap * heap )
: m_heap(heap)
{

}

FrameManager::~FrameManager()
{
}

uint FrameManager::GetFrameBufferColorSize( const FrameDesc* desc )
{
	const uint pixelCount = desc->width * desc->height;
	return pixelCount * sizeof( SColor );
}

uint FrameManager::GetFrameBufferDepthSize( const FrameDesc* desc )
{
	const uint pixelCount = desc->width * desc->height;
	return pixelCount * sizeof( float );
}

FrameBuffer* FrameManager::CreateFrameBuffer( const FrameDesc* desc )
{
	const uint pixelCount = desc->width * desc->height;
	const uint colorSize = GetFrameBufferColorSize( desc );
	const uint depthSize = GetFrameBufferDepthSize( desc );
	
	u8* buffer = (u8*)m_heap->alloc( sizeof( FrameBuffer ) + colorSize + depthSize );

	FrameBuffer* frameBuffer = (FrameBuffer*)buffer;
	buffer += sizeof( FrameBuffer );
	frameBuffer->colors = (SColor*)buffer;
	buffer += colorSize;
	frameBuffer->depths = (float*)buffer;

	frameBuffer->width = desc->width;
	frameBuffer->height = desc->height;

	return frameBuffer;
}

void FrameManager::ReleaseFrameBuffer( FrameBuffer* frameBuffer )
{
	m_heap->free( frameBuffer );
}

END_V6_CORE_NAMESPACE