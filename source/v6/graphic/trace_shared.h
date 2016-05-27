/*V6*/

#ifndef __V6_HLSL_TRACE_SHARED_H__
#define __V6_HLSL_TRACE_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_BLOCK_SHOW_FLAG_MIPS					1
#define HLSL_BLOCK_SHOW_FLAG_BUCKETS				2
#define HLSL_BLOCK_SHOW_FLAG_HISTORY				4

#define HLSL_BLOCK_THREAD_GROUP_SIZE				64

#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFFFFFFFF

#define HLSL_COLOR_SLOT								0

#define HLSL_BLOCK_POS_SLOT							0
#define HLSL_BLOCK_DATA_SLOT						1
#define HLSL_BLOCK_RANGE_SLOT						2
#define HLSL_BLOCK_GROUP_SLOT						3
#define HLSL_BLOCK_CELL_ITEM_SLOT					4
#define HLSL_BLOCK_CELL_ITEM_COUNT_SLOT				5
#define HLSL_CULL_STATS_SLOT						6
#define HLSL_TRACE_CELLS_SLOT						7
#define HLSL_TRACE_INDIRECT_ARGS_SLOT				8
#define HLSL_TRACE_STATS_SLOT						9

#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT		2
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT		(1 << HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT)
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK		(HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT-1)
#define HLSL_CELL_ITEM_PAGE_COUNT					8
#define HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT			(HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * HLSL_CELL_ITEM_PAGE_COUNT)

CBUFFER( CBCull, 0 )
{
	uint				c_cullGridMacroShift;
	float				c_cullInvGridWidth;
	uint				c_cullBlockGroupOffset;
	uint				c_cullBlockRangeOffset;
	float4				c_cullCentersAndGridScales[HLSL_MIP_MAX_COUNT];
	float4				c_cullFrustumPlanes[4];
	
};

struct BlockPerEye
{
	row_major matrix	objectToView;
	row_major matrix	viewToProj;
	
	float3				org;
	float				pad;

	float3				rayDirBase;
	float				pad1;
	
	float3				rayDirUp;
	float				pad2;
	
	float3				rayDirRight;
	float				pad3;
};

CBUFFER( CBBlock, 1 )
{	
	float4				c_blockGridScales[HLSL_MIP_MAX_COUNT];
	float4				c_blockGridCenters[HLSL_MIP_MAX_COUNT];

	uint				c_blockGridMacroShift;
	float				c_blockInvGridWidth;
	uint				c_blockEyeCount;
	uint				c_blockPad;

	float2				c_blockFrameSize;
	uint				c_blockGetStats;
	uint				c_blockShowFlag;

	BlockPerEye			c_blockEyes[2];
};

CBUFFER( CBPixel, 2 )
{
	uint				c_pixelEye;
	uint				c_pixelEyeCount;
	uint2				c_pixelFrameSize;
	float3				c_pixelBackColor;
	float				c_pixelPad;
	
};


struct BlockRange
{
	int3	macroGridOffset;
	uint	frameDistance;
	uint	firstThreadID;
	uint	blockCount;
	uint	blockPosOffset;
	uint	blockDataOffset;
};

struct BlockCellItem
{
	float	depth;
	uint	r8g8b8_hitMask8;
};

struct BlockCullStats 
{
	uint	blockInputCount;
	uint	blockProcessedCount;
	uint	blockPassedCount;
	uint	cellOutputCount;
};

struct BlockTraceStats 
{
	uint	cellInputCount;
	uint	cellProcessedCount;
	uint	pixelSampleCount;
	uint	cellItemCount;
	uint	cellItemMaxCountPerPixel;
};

#define trace_cellGroupCountX_offset( BUCKET )				(BUCKET * 3 + 0)
#define trace_cellGroupCountY_offset( BUCKET )				(BUCKET * 3 + 1)
#define trace_cellGroupCountZ_offset( BUCKET )				(BUCKET * 3 + 2)
#define trace_blockOffset_offset( BUCKET )					(trace_cellGroupCountZ_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define trace_blockCount_offset( BUCKET )					(trace_blockOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define trace_all_offset									trace_blockCount_offset( HLSL_BUCKET_COUNT )

#define trace_cellGroupCountX( BUCKET )						traceIndirectArgs[trace_cellGroupCountX_offset( BUCKET )]
#define trace_cellGroupCountY( BUCKET )						traceIndirectArgs[trace_cellGroupCountY_offset( BUCKET )]
#define trace_cellGroupCountZ( BUCKET )						traceIndirectArgs[trace_cellGroupCountZ_offset( BUCKET )]
#define trace_blockOffset( BUCKET )							traceIndirectArgs[trace_blockOffset_offset( BUCKET )]
#define trace_blockCount( BUCKET )							traceIndirectArgs[trace_blockCount_offset( BUCKET )]

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_TRACE_SHARED_H__
