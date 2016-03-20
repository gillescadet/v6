/*V6*/

#pragma once

#ifndef __V6_CORE_CODEC_H__
#define __V6_CORE_CODEC_H__

#include <v6/core/vec3.h> 

BEGIN_V6_CORE_NAMESPACE

#define CODEC_STREAM_MAGIC			"V6_S"
#define CODEC_STREAM_VERSION		0

#define CODEC_FRAME_MAGIC			"V6_F"
#define CODEC_FRAME_VERSION			3

#define CODEC_BUCKET_COUNT			5
#define CODEC_CELL_MAX_COUNT		64
#define CODEC_MIP_MAX_COUNT			16

class IAllocator;
class IStreamReader;
class IStreamWriter;

struct CodecFrameDesc_s
{
	Vec3	origin;
	u32		frame;
	u32		sampleCount;
	u32		gridMacroShift;
	float	gridScaleMin;
	float	gridScaleMax;
	u32		blockCounts[CODEC_BUCKET_COUNT];
};

struct CodecFrameData_s
{
	void*	blockPos;
	void*	blockData;
};

u32		Codec_GetMipCount( const CodecFrameDesc_s* desc );
bool	Codec_ReadFrame( IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, IAllocator* allocator );
void	Codec_WriteFrame( IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_CODEC_H__
