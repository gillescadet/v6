/*V6*/

#pragma once

#ifndef __V6_CODEC_CODEC_H__
#define __V6_CODEC_CODEC_H__

#include <v6/core/mat4x4.h> 
#include <v6/core/vec3.h> 
#include <v6/core/vec3i.h> 

BEGIN_V6_NAMESPACE

#define CODEC_RAWFRAME_MAGIC				"V6RF"
#define CODEC_RAWFRAME_VERSION				7

#define CODEC_FRAME_MAGIC					"V6F\0"
#define CODEC_FRAME_VERSION					6

#define CODEC_SEQUENCE_MAGIC				"V6S\0"
#define CODEC_SEQUENCE_VERSION				2

#define CODEC_STREAM_MAGIC					"V6\0\0"
#define CODEC_STREAM_VERSION				2
#define CODEC_STREAM_KEY_MAX_COUNT			32
#define CODEC_STREAM_HEADER_SIZE			MulKB( 72u )

#define CODEC_HEAD_ROOM_SIZE				50.0f
#define CODEC_ICON_WIDTH					256

#define CODEC_RAWFRAME_BUCKET_COUNT			5
#define CODEC_CELL_MAX_COUNT				64u
#define CODEC_MIP_MAX_COUNT					16u
#define CODEC_FACE_MAX_COUNT				8u
#define CODEC_GRID_MAX_COUNT				(CODEC_MIP_MAX_COUNT > CODEC_FACE_MAX_COUNT ? CODEC_MIP_MAX_COUNT : CODEC_FACE_MAX_COUNT)
#define CODEC_RANGE_MAX_COUNT				(1u << 14)
#define CODEC_FRAME_MAX_COUNT				128
#define CODEC_INVALID_FRAME_RANK			CODEC_FRAME_MAX_COUNT
#define CODEC_BLOCK_THREAD_GROUP_SIZE		64
#define CODEC_BLOCK_MAX_COUNT_PER_SEQUENCE	MulMB( 12u )

#define CODEC_MIP_MACRO_XYZ_BIT_COUNT		9
#define CODEC_MIP_MACRO_XYZ_BIT_MASK		((1 << CODEC_MIP_MACRO_XYZ_BIT_COUNT) - 1)
#define CODEC_ONION_MACRO_Z_BIT_COUNT		11

#define CODEC_CLUSTER_SIZE					4096
#define CODEC_BUFFER_ALIGNMENT				16

#define CODEC_COLOR_ERROR_TOLERANCE			1
#define CODEC_COLOR_COUNT_TOLERANCE			4

#define CODEC_FRAME_COMPRESS_TYPE_NONE		0
#define CODEC_FRAME_COMPRESS_TYPE_LZ4		1
#define CODEC_FRAME_COMPRESS_TYPE_ZSTD		2

#if V6_UE4_PLUGIN == 1
#define CODEC_FRAME_COMPRESS				0
#else
#define CODEC_FRAME_COMPRESS				CODEC_FRAME_COMPRESS_TYPE_ZSTD
#endif
#define CODEC_FRAME_PACK_POSITIONS			1

#define	CODEC_ICON_KEY						"icon"
#define	CODEC_ICON_MAGIC					"BC1\0"

#include <v6/core/mat4x4.h>

class CStreamReaderWithBuffering;
class CStreamWriterWithBuffering;
class IAllocator;
class IStack;
class IStreamReader;
class IStreamWriter;

enum
{
	CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW = 1 << 0,
};

enum
{
	CODEC_FRAME_FLAG_MOTION = 1 << 0,
};

struct CodecStreamDesc_s
{
	Vec3			gridOrigin;
	u32				sequenceCount;
	u32				frameCount;
	u32				frameRate;
	u32				playRate;
	u32				gridWidth;
	float			gridScaleMin;
	float			gridScaleMax;
	u32				flags;
	u32				maxBlockCountPerSequence;
	u32				maxBlockRangeCountPerFrame;
	u32				maxBlockGroupCountPerFrame;
	u32				keyCount;
	u32				keySizes[CODEC_STREAM_KEY_MAX_COUNT];
	u32				valueSizes[CODEC_STREAM_KEY_MAX_COUNT];
};

struct CodecStreamData_s
{
	char*			keys;
	u8*				values;
};

struct CodecSequenceDesc_s
{
	u32				sequenceID;
	u32				frameCount;
};

struct CodecRawFrameDesc_s
{
	Vec3			gridOrigin;
	float			gridYaw;
	u32				frameID;
	u32				frameRate;
	u32				gridWidth;
	float			gridScaleMin;
	float			gridScaleMax;
	u32				flags;
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

struct CodecBlockRange_s
{
	u32				frameRank7_newBlock1_firstBlockID24;
	u32				blockCount;
};

struct CodecFrameData_s
{	
	u32*				blockPos;
	u32*				blockCellPresences0;
	u32*				blockCellPresences1;
	u32*				blockCellEndColors;
	u32*				blockCellColorIndices0;
	u32*				blockCellColorIndices1;
	u32*				blockCellColorIndices2;
	u32*				blockCellColorIndices3;
	CodecBlockRange_s*	blockRanges;
};

u32		Codec_AlignToClusterSize( u32 size );
u64		Codec_AlignToClusterSize( u64 size );
void*	Codec_AlignToClusterSize( void* p );
void*	Codec_AllocToClusterSizeAndFillPaddingWithZero( void** buffer, u64 size, IAllocator* allocator );
u32		Codec_GetClusterSize();
Vec3	Codec_ComputeGridCenter( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth );
Vec3i	Codec_ComputeMacroGridCoords( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth );
u32		Codec_GetMipCount( float gridScaleMin, float gridScaleMax );
void*	Codec_ReadFrame( CStreamReaderWithBuffering* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack );
bool	Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator );
bool	Codec_ReadRawFrameDesc( IStreamReader* streamReader, CodecRawFrameDesc_s* desc );
bool	Codec_ReadSequenceDesc( IStreamReader* streamReader, CodecSequenceDesc_s* desc, u32 sequenceID );
void*	Codec_ReadStreamDesc( IStreamReader* streamReader, CodecStreamDesc_s* desc, CodecStreamData_s* data, IAllocator* allocator );
bool	Codec_WriteFrame( CStreamWriterWithBuffering* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, IStack* stack );
void	Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator );
void	Codec_WriteSequenceDesc( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc );
bool	Codec_WriteStreamDesc( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc, const CodecStreamData_s* data, IStack* stack );

END_V6_NAMESPACE

#endif // __V6_CODEC_CODEC_H__
