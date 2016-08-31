/*V6*/

#pragma once

#ifndef __V6_CODEC_CODEC_H__
#define __V6_CODEC_CODEC_H__

#include <v6/core/mat4x4.h> 
#include <v6/core/vec3.h> 
#include <v6/core/vec3i.h> 

BEGIN_V6_NAMESPACE

#define CODEC_RAWFRAME_MAGIC				"V6RF"
#define CODEC_RAWFRAME_VERSION				6

#define CODEC_FRAME_MAGIC					"V6F"
#define CODEC_FRAME_VERSION					5

#define CODEC_SEQUENCE_MAGIC				"V6S"
#define CODEC_SEQUENCE_VERSION				2

#define CODEC_STREAM_MAGIC					"V6"
#define CODEC_STREAM_VERSION				1

#define CODEC_RAWFRAME_BUCKET_COUNT			5
#define CODEC_CELL_MAX_COUNT				64u
#define CODEC_MIP_MAX_COUNT					16
#define CODEC_RANGE_MAX_COUNT				65536
#define CODEC_FRAME_MAX_COUNT				128
#define CODEC_INVALID_FRAME_RANK			CODEC_FRAME_MAX_COUNT
#define CODEC_BLOCK_THREAD_GROUP_SIZE		64
#define CODEC_BLOCK_MAX_COUNT_PER_SEQUENCE	MulMB( 16u )

#define CODEC_COLOR_ERROR_TOLERANCE			1
#define CODEC_COLOR_COUNT_TOLERANCE			4

#if V6_UE4_PLUGIN == 1
#define CODEC_FRAME_COMPRESS				0
#else
#define CODEC_FRAME_COMPRESS				1
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
	u32				frameRank7_mip4_blockCount21;
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
	u32				maxBlockCountPerSequence;
	u32				maxBlockRangeCountPerFrame;
	u32				maxBlockGroupCountPerFrame;
};

struct CodecSequenceDesc_s
{
	u32				sequenceID;
	u32				frameCount;
	u32				rangeDefCount;
};

struct CodecSequenceData_s
{
	CodecRange_s*	rangeDefs;
};

struct CodecRawFrameDesc_s
{
	Vec3			gridOrigin;
	float			gridYaw;
	u32				frameID;
	u32				frameRate;
	u32				sampleCount;
	u32				gridMacroShift;
	float			gridScaleMin;
	float			gridScaleMax;
	u32				blockCounts[CODEC_RAWFRAME_BUCKET_COUNT];
};

struct CodecRawFrameData_s
{
	void*			blockPos;
	void*			blockData;
};

struct CodecRawFrameBuffer_s
{
	void*			blockPosBuffer;
	void*			blockDataBuffer;
};

struct CodecFrameDesc_s
{
	Vec3			gridOrigin;
	float			gridYaw;
	u16				frameRank;
	u16				flags;
	u32				blockCount;
	u32				blockRangeCount;
};

struct CodecFrameData_s
{	
	u32*			blockPos;
	u32*			blockCellPresences0;
	u32*			blockCellPresences1;
	u32*			blockCellEndColors;
	u32*			blockCellColorIndices0;
	u32*			blockCellColorIndices1;
	u32*			blockCellColorIndices2;
	u32*			blockCellColorIndices3;
	u16*			rangeIDs; // optim: this could replaced with firstRangeID and rangeCount
};

u32		Codec_AlignToClusterSize( u32 size );
u64		Codec_AlignToClusterSize( u64 size );
void*	Codec_AlignToClusterSize( void* p );
void*	Codec_AllocToClusterSizeAndFillPaddingWithZero( void** buffer, u64 size, IAllocator* allocator );
u32		Codec_GetClusterSize();
Vec3	Codec_ComputeGridCenter( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth );
Vec3i	Codec_ComputeMacroGridCoords( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth );
u32		Codec_GetMipCount( float gridScaleMin, float gridScaleMax );
void*	Codec_ReadFrame( IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack );
bool	Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator );
bool	Codec_ReadRawFrameDesc( IStreamReader* streamReader, CodecRawFrameDesc_s* desc );
void*	Codec_ReadSequence( IStreamReader* streamReader, CodecSequenceDesc_s* desc, CodecSequenceData_s* data, u32 sequenceID, IAllocator* alllocator );
bool	Codec_ReadStreamDesc( IStreamReader* streamReader, CodecStreamDesc_s* desc );
bool	Codec_WriteFrame( IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, IStack* stack );
void	Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator );
void	Codec_WriteSequence( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc, const CodecSequenceData_s* data );
void	Codec_WriteStreamDesc( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc );

END_V6_NAMESPACE

#endif // __V6_CODEC_CODEC_H__
