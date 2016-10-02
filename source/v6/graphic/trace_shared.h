/*V6*/

#ifndef __V6_HLSL_TRACE_SHARED_H__
#define __V6_HLSL_TRACE_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_TRACE_DEBUG							0

#define HLSL_TRACE_STATE_HIT						 0
#define HLSL_TRACE_STATE_MISS_BLOCK					-1
#define HLSL_TRACE_STATE_MISS_CELL					-2

#define HLSL_BLOCK_SHOW_FLAG_GRIDS					1
#define HLSL_BLOCK_SHOW_FLAG_HISTORY				2
#define HLSL_BLOCK_SHOW_FLAG_OVERDRAW				4
#define HLSL_BLOCK_SHOW_FLAG_BLOCK					8

#define HLSL_BLOCK_THREAD_GROUP_SIZE				64

#define HLSL_BLOCK_DEBUG_BOX_MAX_COUNT				(64 * 1024)

#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFFFFFFFF

#define HLSL_BILINEAR_SLOT							0

#define HLSL_COLOR_SLOT								0
#define HLSL_DEPTH24_SLOT							1
#define HLSL_DISPLACEMENT_SLOT						2
#define HLSL_HISTORY_SLOT							3

#define HLSL_VISIBLE_BLOCK_SLOT						0
#define HLSL_VISIBLE_BLOCK_CONTEXT_SLOT				1
#define HLSL_BLOCK_PATCH_COUNTERS_SLOT				2
#define HLSL_BLOCK_PATCHES_SLOT						3

#define HLSL_CULL_STATS_SLOT						4
#define HLSL_PROJECT_STATS_SLOT						4
#define HLSL_TRACE_STATS_SLOT						4
#define HLSL_TRACE_DEBUG_BOX_SLOT					5

#define HLSL_BLOCK_POS_SLOT							10
#define HLSL_BLOCK_CELL_PRESENCE0_SLOT				11
#define HLSL_BLOCK_CELL_PRESENCE1_SLOT				12
#define HLSL_BLOCK_CELL_END_COLOR_SLOT				13
#define HLSL_BLOCK_CELL_COLOR_INDEX0_SLOT			14
#define HLSL_BLOCK_CELL_COLOR_INDEX1_SLOT			15
#define HLSL_BLOCK_CELL_COLOR_INDEX2_SLOT			16
#define HLSL_BLOCK_CELL_COLOR_INDEX3_SLOT			17
#define HLSL_BLOCK_RANGE_SLOT						18
#define HLSL_BLOCK_GROUP_SLOT						19
#define HLSL_BLOCK_GRID_MACRO_OFFSET_SLOT			20

#define HLSL_BLOCK_PAGE_MAX_COUNT					16
#define HLSL_BLOCK_PATCH_MAX_COUNT_PER_TILE			(64 * HLSL_BLOCK_PAGE_MAX_COUNT)

CBUFFER( CBCull, 0 )
{
	float				c_cullInvGridWidth;
	uint				c_cullFrameRank;
	uint				c_cullFrameChanged;
	uint				c_cullPad0;

	float				c_cullGridMinScale;
	float				c_cullInvMacroPeriodWidth;
	float				c_cullInvMacroGridWidth;
	float				c_cullPad1;

	float4				c_cullGridCenter;

	float4				c_cullCentersAndGridScales[HLSL_MIP_MAX_COUNT];
	float4				c_cullFrustumPlanes[4]; // w is pre-negated
};

CBUFFER( CBProject, 1 )
{
	float2				c_projectFrameSize;
	uint2				c_projectFrameTileSize;

	float				c_projectInvGridWidth;
	float3				c_projectPad0;

	float				c_projectGridMinScale;
	float				c_projectInvMacroPeriodWidth;
	float				c_projectInvMacroGridWidth;
	float				c_projectPad1;

	float4				c_projectGridCenter;

	float4				c_projectCentersAndGridScales[HLSL_MIP_MAX_COUNT];

	float4				c_projectPrevWorldToProjX;
	float4				c_projectPrevWorldToProjY;
	float4				c_projectPrevWorldToProjW;
	float4				c_projectCurWorldToProjX;
	float4				c_projectCurWorldToProjY;
	float4				c_projectCurWorldToProjW;
};

CBUFFER( CBTrace, 2 )
{	
	float				c_traceGridMinScale;
	float				c_traceInvMacroPeriodWidth;
	float				c_traceInvMacroGridWidth;
	float				c_tracePad0;

	float4				c_traceGridCenter;

	float4				c_traceGridScales[HLSL_MIP_MAX_COUNT];
	float4				c_traceGridCenters[HLSL_MIP_MAX_COUNT];

	uint				c_tracePad1;
	uint				c_traceEyeCount;
	float2				c_traceJitter;

	float2				c_traceFrameSize;
	uint2				c_traceFrameTileSize;

	float4				c_traceRayOrg;
	float4				c_traceRayDirBase;
	float4				c_traceRayDirUp;
	float4				c_traceRayDirRight;
	
	uint				c_traceGetStats;
	uint				c_traceShowFlag;
	uint2				c_tracePad2;
};

CBUFFER( CBTSAA, 2 )
{
	float2				c_tsaaJitter;
	float				c_tsaaBlendFactor;
	float				c_tsaaPad;
	float2				c_tsaaFrameSize;
	float2				c_tsaaInvFrameSize;
};

CBUFFER( CBPostProcess, 3 )
{
	float				c_postProcessFadeToBlack;
	float3				c_postProcessPad;
};

struct BlockGroup
{
	uint	rangeID14_firstGroupID18;
};

struct BlockRange
{
	uint	frameRank7_newBlock1_firstBlockID24;
	uint	blockCount;
};

struct GridMacroOffset
{
	int3	offsets[HLSL_GRID_MAX_COUNT];
};

struct VisibleBlockMip
{
	uint	newBlock1_blockPosID;
	uint	mip4_none1_blockPos27;
};

struct VisibleBlockOnion
{
	uint	newBlock1_blockPosID;
	uint	sign1_axis2_z11_y9_x9;
};

struct VisibleBlockContext
{
	uint	count;
	uint	groupCountX;
	uint	groupCountY;
	uint	groupCountZ;
	uint	minDistance;
	float	fadeToBlack;
};

struct BlockPatchMip
{
	uint	newBlock1_blockPosID;
	uint	mip4_none1_blockPos27;
	uint	none4_cellmin222_cellmax222_x4_y4_w4_h4;
	uint	xdsp16_ydsp16;
};

struct BlockPatchOnion
{
	uint	newBlock1_blockPosID;
	uint	sign1_axis2_z11_y9_x9;
	uint	none4_cellmin222_cellmax222_x4_y4_w4_h4;
	uint	xdsp16_ydsp16;
};

struct BlockCullStats 
{
	uint	blockInputCount;
	uint	blockProcessedCount;
	uint	blockPassedCount;
	uint	assertFailedBits;
	uint4	assertDataU32[4];
	float4	assertDataF32[4];
};

struct BlockProjectStats 
{
	uint	blockInputCount;
	uint	blockProcessedCount;
	uint	blockPatchHeaderPixelCount;
	uint	blockPatchHeaderCount;
	uint	blockPatchDetailCount;
	uint	blockPatchDetailPixelCount;
	uint	blockPatchMaxPage;
};

struct BlockDebugBox
{
	float3	boxMinRS;
	float3	boxMaxRS;
};

struct BlockTraceStats 
{
	uint			tileInputCounts[HLSL_BLOCK_PAGE_MAX_COUNT];
	uint			patchInputCount;
	uint			pixelInputCount;
	uint			pixelTraceCount;
	uint			pixelEmptyMaskCount;
	uint			pixelNotEmptyMaskCount;
	uint			pixelHitCounts[3];
	uint			pixelDoneCount;
	uint			pixelPageCount;
	uint			pixelStepCount;
	uint			pixelStepMaxCount;
	uint			assertFailedBits;
	uint4			assertDataU32[4];
	float4			assertDataF32[4];
	float3			debugRayDir;
	uint			debugBoxCount;
};

#define VISIBLEBLOCKCONTEXT_COUNT_OFFSET		0
#define VISIBLEBLOCKCONTEXT_GROUPCOUNTX_OFFSET	1
#define VISIBLEBLOCKCONTEXT_GROUPCOUNTY_OFFSET	2
#define VISIBLEBLOCKCONTEXT_GROUPCOUNTZ_OFFSET	3

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_TRACE_SHARED_H__
