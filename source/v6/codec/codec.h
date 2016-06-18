/*V6*/

#pragma once

#ifndef __V6_CODEC_CODEC_H__
#define __V6_CODEC_CODEC_H__

#include <v6/core/mat4x4.h> 
#include <v6/core/vec3.h> 
#include <v6/core/vec3i.h> 

BEGIN_V6_NAMESPACE

#define CODEC_RAWFRAME_MAGIC			"V6RF"
#define CODEC_RAWFRAME_VERSION			4

#define CODEC_FRAME_MAGIC				"V6F"
#define CODEC_FRAME_VERSION				4

#define CODEC_SEQUENCE_MAGIC			"V6S"
#define CODEC_SEQUENCE_VERSION			2

#define CODEC_STREAM_MAGIC				"V6"
#define CODEC_STREAM_VERSION			0

#define CODEC_BUCKET_COUNT				5
#define CODEC_CELL_MAX_COUNT			64
#define CODEC_MIP_MAX_COUNT				16
#define CODEC_RANGE_MAX_COUNT			65536
#define CODEC_FRAME_MAX_COUNT			256
#define CODEC_BLOCK_THREAD_GROUP_SIZE	64

#define CODEC_COLOR_ERROR_TOLERANCE		15
#define CODEC_COLOR_COUNT_TOLERANCE		4
#define CODEC_COLOR_COMPRESS			0

#if V6_UE4_PLUGIN == 1
#define CODEC_FRAME_COMPRESS			0
#else
#define CODEC_FRAME_COMPRESS			1
#endif

#include <v6/core/mat4x4.h>

class IAllocator;
class IStack;
class IStreamReader;
class IStreamWriter;

enum
{
	CODEC_FRAME_FLAG_MOTION = 1 << 0,
};

struct CodecRange_s
{
	u32				frameRank8_mip4_blockCount20;
};

struct CodecStreamDesc_s
{
	u32				sequenceCount;
	u32				frameCount;
	u32				frameRate;
	u32				playRate;
	u32				sampleCount;
	u32				gridMacroShift;
	float			gridScaleMin;
	float			gridScaleMax;
	u32				maxBlockPosCountPerSequence;
	u32				maxBlockDataCountPerSequence;
	u32				maxBlockRangeCountPerFrame;
	u32				maxBlockCountPerFrame;
	u32				maxBlockGroupCountPerFrame;
};

struct CodecStreamData_s
{
	u32*			frameOffsets;
	u32*			sequenceByteOffsets;
};

struct CodecSequenceDesc_s
{
	u32				sequenceID;
	u32				frameCount;
	u32				rangeDefCounts[CODEC_BUCKET_COUNT];
};

struct CodecSequenceData_s
{
	CodecRange_s*	rangeDefs;
};

struct CodecRawFrameDesc_s
{
	Vec3			gridOrigin;
	Vec3			gridBasis[3];
	u32				frameID;
	u32				frameRate;
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

struct CodecFrameDesc_s
{
	Vec3			gridOrigin;
	Vec3			gridBasis[3];
	u16				frameRank;
	u16				flags;
	u32				blockCounts[CODEC_BUCKET_COUNT];
	u32				blockRangeCounts[CODEC_BUCKET_COUNT];
};

struct CodecFrameData_s
{	
	u32*			blockPos;
	u32*			blockData;
	u16*			rangeIDs;
};

Vec3	Codec_ComputeGridCenter( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth );
Vec3i	Codec_ComputeMacroGridCoords( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth );
u32		Codec_GetMipCount( float gridScaleMin, float gridScaleMax );
void*	Codec_ReadFrame( IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack );
bool	Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, IAllocator* allocator );
bool	Codec_ReadRawFrameHeader( IStreamReader* streamReader, CodecRawFrameDesc_s* desc );
void*	Codec_ReadSequence( IStreamReader* streamReader, CodecSequenceDesc_s* desc, CodecSequenceData_s* data, u32 sequenceID, IAllocator* alllocator );
void*	Codec_ReadStream( IStreamReader* streamReader, CodecStreamDesc_s* desc, CodecStreamData_s* data, IAllocator* allocator );
bool	Codec_WriteFrame( IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, IStack* stack );
void	Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data );
void	Codec_WriteSequence( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc, const CodecSequenceData_s* data );
void	Codec_WriteStream( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc, const CodecStreamData_s* data );

END_V6_NAMESPACE

#endif // __V6_CODEC_CODEC_H__
