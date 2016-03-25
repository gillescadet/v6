/*V6*/

#pragma once

#ifndef __V6_CORE_CODEC_H__
#define __V6_CORE_CODEC_H__

#include <v6/core/vec3.h> 
#include <v6/core/vec3i.h> 

BEGIN_V6_CORE_NAMESPACE

#define CODEC_STREAM_MAGIC				"V6_S"
#define CODEC_STREAM_VERSION			0

#define CODEC_RAWFRAME_MAGIC			"V6_F"
#define CODEC_RAWFRAME_VERSION			3

#define CODEC_IFRAME_MAGIC				"V6IF"
#define CODEC_IFRAME_VERSION			0

#define CODEC_PFRAME_MAGIC				"V6PF"
#define CODEC_PFRAME_VERSION			0

#define CODEC_BUCKET_COUNT				5
#define CODEC_CELL_MAX_COUNT			64
#define CODEC_MIP_MAX_COUNT				16

class IAllocator;
class IStreamReader;
class IStreamWriter;

struct CodecStreamDesc_s
{
	u32				frameCount;
	u32				sampleCount;
	u32				gridMacroShift;
	float			gridScaleMin;
	float			gridScaleMax;
};

struct CodecRawFrameDesc_s
{
	Vec3			origin;
	u32				frame;
	u32				sampleCount;
	u32				gridMacroShift;
	float			gridScaleMin;
	float			gridScaleMax;
	u32				blockCounts[CODEC_BUCKET_COUNT];
};

struct CodecRawFrameData_s
{
	void*			blockPos;
	void*			blockData;
};

struct CodecRange_s
{
	Vec3i			gridOrg;
	u32				blockOffset;
};

struct CodecIFrameDesc_s
{
	Vec3			origin;
	u32				frame;
	u32				rangeCounts[CODEC_BUCKET_COUNT];
	u32				dataBlockCounts[CODEC_BUCKET_COUNT];
	u32				usedBlockCounts[CODEC_BUCKET_COUNT];
};

struct CodecIFrameData_s
{
	CodecRange_s*	ranges;
	u32*			blockPos;
	u32*			blockData;
	u16*			groups;
};

struct CodecPFrameDesc_s
{
	Vec3			origin;
	u32				frame;
	u32				dataBlockCounts[CODEC_BUCKET_COUNT];
	u32				usedBlockCounts[CODEC_BUCKET_COUNT];
};

struct CodecPFrameData_s
{
	u32*			blockPos;
	u32*			blockData;
	u16*			groups;
};

u32		Codec_GetMipCount( float gridScaleMin, float gridScaleMax );
bool	Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, IAllocator* allocator );
void	Codec_WriteStreamHeader( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc );
void	Codec_WriteIFrame( IStreamWriter* streamWriter, const CodecIFrameDesc_s* desc, const CodecIFrameData_s* data );
void	Codec_WritePFrame( IStreamWriter* streamWriter, const CodecPFrameDesc_s* desc, const CodecPFrameData_s* data );
void	Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_CODEC_H__
