/*V6*/

#pragma once

#ifndef __V6_CORE_FRAME_MANAGER_H__
#define __V6_CORE_FRAME_MANAGER_H__

#include <v6/core/color.h>
#include <v6/core/types.h>

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

struct FrameDesc
{
	uint width;
	uint height;
};

struct FrameBuffer
{
	Color_s* colors;
	float* depths;
	uint width;
	uint height;
};

class FrameManager
{
public:
	FrameManager( IAllocator* heap );
	~FrameManager();

public:
	static uint GetFrameBufferColorSize( const FrameDesc* desc );
	static uint GetFrameBufferDepthSize( const FrameDesc* desc );

public:
	FrameBuffer* CreateFrameBuffer( const FrameDesc* desc );	
	void ReleaseFrameBuffer( FrameBuffer* frameBuffer );

private:
	IAllocator* m_heap;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_FRAME_MANAGER_H__