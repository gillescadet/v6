/*V6*/

#pragma once

#ifndef __V6_VIEWER_CODEC_H__
#define __V6_VIEWER_CODEC_H__

#include <v6/core/vec3.h> 

BEGIN_V6_CORE_NAMESPACE

class IStreamReader;
class IStreamWriter;

END_V6_CORE_NAMESPACE

BEGIN_V6_VIEWER_NAMESPACE

#define CODEC_STREAM_MAGIC			"V6_S"
#define CODEC_STREAM_VERSION		0

#define CODEC_FRAME_MAGIC			"V6_F"
#define CODEC_FRAME_VERSION			2

#define CODEC_BUCKET_COUNT			5

struct CodecFrameDesc_s
{
	core::Vec3	origin;
	core::u32	frame;
	core::u32	sampleCount;
	core::u32	gridResolution;
	float		gridScaleMin;
	float		gridScaleMax;
	core::u32	blockCounts[CODEC_BUCKET_COUNT];
};

struct CodecFrameData_s
{
	void* blockPos;
	void* blockData;
};

bool Codec_ReadFrame( core::IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, core::IAllocator* allocator );
void Codec_WriteFrame( core::IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data );

END_V6_VIEWER_NAMESPACE

#endif // __V6_VIEWER_CODEC_H__
