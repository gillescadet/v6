/*V6*/

#ifndef __V6_HLSL_TRACE_SHARED_H__
#define __V6_HLSL_TRACE_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_BLOCK_SHOW_FLAG_MIPS					1
#define HLSL_BLOCK_SHOW_FLAG_BUCKETS				2
#define HLSL_BLOCK_SHOW_FLAG_HISTORY				4

#define HLSL_BLOCK_THREAD_GROUP_SIZE				64
#define HLSL_BLOCK_TRACE_CELL_STATS_MAX_COUNT		(10 * 1024)

#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFFFFFFFF

#define HLSL_BILINEAR_SLOT							0

#define HLSL_COLOR_SLOT								0
#define HLSL_DISPLACEMENT_SLOT						1
#define HLSL_HISTORY_SLOT							2

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
#define HLSL_TRACE_CELL_STATS_SLOT					10

#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT		2
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT		(1 << HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT)
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK		(HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT-1)
#define HLSL_CELL_ITEM_PAGE_COUNT					8
#define HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT			(HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * HLSL_CELL_ITEM_PAGE_COUNT)

CBUFFER( CBCull, 0 )
{
	uint				c_cullGridMacroShift;
	float				c_cullInvGridWidth;
	uint2				c_cullPad0;

	uint				c_cullBlockGroupCount;
	uint				c_cullBlockGroupOffset;
	uint				c_cullBlockRangeOffset;
	uint				c_cullPad1;

	float4				c_cullCentersAndGridScales[HLSL_MIP_MAX_COUNT];
	float4				c_cullFrustumPlanes[4];
	
};

struct TracePerEye
{
	float4				prevWorldToProj[4];
	float4				curWorldToProj[4];
	
	float3				org;
	float				pad0;

	float3				rayDirBase;
	float				pad1;
	
	float3				rayDirUp;
	float				pad2;
	
	float3				rayDirRight;
	float				pad3;
};

CBUFFER( CBTrace, 1 )
{	
	float4				c_traceGridScales[HLSL_MIP_MAX_COUNT];
	float4				c_traceGridCenters[HLSL_MIP_MAX_COUNT];

	uint				c_traceGridMacroShift;
	uint				c_traceEyeCount;
	float2				c_traceJitter;

	float2				c_traceFrameSize;
	uint				c_traceGetStats;
	uint				c_traceShowFlag;

	TracePerEye			c_traceEyes[2];
	
};

CBUFFER( CBBlend, 2 )
{
	uint				c_blendEye;
	uint				c_blendEyeCount;
	uint2				c_blendFrameSize;
	float3				c_blendBackColor;
	float				c_blendPad;
};

CBUFFER( CBFilter, 3 )
{
	float2				c_filterJitter;
	float				c_filterBlendFactor;
	float				c_filterPad;
	float2				c_filterFrameSize;
	float2				c_filterInvFrameSize;
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
	uint	hitMask1_depth31;
	uint	r8g8b8_hitMask8;
	uint	xdsp16_ydsp16;
};

struct BlockCullStats 
{
	uint	blockInputCount;
	uint	blockProcessedCount;
	uint	blockPassedCount;
	uint	cellOutputCounts[HLSL_MIP_MAX_COUNT];
};

struct BlockTraceStats 
{
	uint	cellInputCount;
	uint	cellProcessedCounts[HLSL_MIP_MAX_COUNT];
	uint	pixelSampleCount;
	uint	cellItemCounts[HLSL_MIP_MAX_COUNT];
	uint	cellItemMaxCountPerPixel;
	uint	traceCellStatCount;
};

struct BlockTraceCellStats
{
	uint	blockCellID;
	int2	pixelCoords;
	int		x;
	int		y;
	float3	boxMinRS;
	float3	boxMaxRS;
	float3	rayDir;
	float	tIn;
	float	tOut;
	bool	hit;
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
