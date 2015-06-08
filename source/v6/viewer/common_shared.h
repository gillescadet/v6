/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_DEBUG_FILL					1

#define HLSL_FIRST_SLOT					0
#define HLSL_COLOR_SLOT					0
#define HLSL_DEPTH_SLOT					1

#define HLSL_GRIDBLOCK_ID_SLOT				2
#define HLSL_GRIDBLOCK_COLOR_SLOT			3
#define HLSL_GRIDBLOCK_POS_SLOT				4
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT	5
#define HLSL_GRIDBLOCK_PACKEDCOLOR_SLOT		6


#define CONCAT( X, Y )						X ## Y

#define HLSL_COLOR_SRV						CONCAT( t, HLSL_COLOR_SLOT )
#define HLSL_DEPTH_SRV						CONCAT( t, HLSL_DEPTH_SLOT )

#define HLSL_GRIDBLOCK_ID_SRV				CONCAT( t, HLSL_GRIDBLOCK_ID_SLOT )
#define HLSL_GRIDBLOCK_COLOR_SRV			CONCAT( t, HLSL_GRIDBLOCK_COLOR_SLOT )
#define HLSL_GRIDBLOCK_POS_SRV				CONCAT( t, HLSL_GRIDBLOCK_POS_SLOT )
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_SRV	CONCAT( t, HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR_SRV		CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR_SLOT )

#define HLSL_GRIDBLOCK_ID_UAV				CONCAT( u, HLSL_GRIDBLOCK_ID_SLOT )
#define HLSL_GRIDBLOCK_COLOR_UAV			CONCAT( u, HLSL_GRIDBLOCK_COLOR_SLOT )
#define HLSL_GRIDBLOCK_POS_UAV				CONCAT( u, HLSL_GRIDBLOCK_POS_SLOT )
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV	CONCAT( u, HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR_UAV		CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR_SLOT )

#define HLSL_GRID_THREAD_GROUP_SIZE			128

#define HLSL_GRID_MACRO_SHIFT				8
#define HLSL_GRID_MACRO_2XSHIFT				(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT				(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_WIDTH				(1 << HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_MASK				(HLSL_GRID_MACRO_WIDTH-1)

#define HLSL_GRID_BLOCK_COUNT				(1 << HLSL_GRID_MACRO_3XSHIFT)
#define HLSL_GRID_BLOCK_SHIFT				2
#define HLSL_GRID_BLOCK_2XSHIFT				(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_3XSHIFT				(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_WIDTH				(1 << HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_MASK				(HLSL_GRID_BLOCK_WIDTH-1)
#define HLSL_GRID_BLOCK_INVALID				uint( -1 )
#define HLSL_GRID_BLOCK_SETTING				uint( -2 )

#define HLSL_GRID_BLOCK_CELL_COUNT			(1 << HLSL_GRID_BLOCK_3XSHIFT)
#define HLSL_GRID_BLOCK_CELL_POS_MASK		(HLSL_GRID_BLOCK_CELL_COUNT-1)

#define HLSL_GRID_SHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_WIDTH						(1 << HLSL_GRID_SHIFT)
#define HLSL_GRID_MASK						(HLSL_GRID_WIDTH-1)
#define HLSL_GRID_INV_WIDTH					(1.0 / HLSL_GRID_WIDTH)
#define HLSL_GRID_HALF_WIDTH				(HLSL_GRID_WIDTH >> 1)

CBUFFER( CBView, 0 )
{
	row_major	matrix objectToView;
	row_major	matrix viewToProj;
	float		zFar;
	int			CBView_pad1;
	uint		frameWidth;
	uint		frameHeight;	
};

CBUFFER( CBGrid, 1 )
{
	float		depthLinearScale;
	float		depthLinearBias;
	float		invFrameSize;
	float		gridScale;
	float		invGridScale;
	float		_pad0;
	float		_pad1;
	float		_pad2;
};

struct GridBlockColor
{
	uint4	colors[HLSL_GRID_BLOCK_CELL_COUNT];
};

struct GridBlockPackedColor
{
	uint	colors[HLSL_GRID_BLOCK_CELL_COUNT];
};

struct GridIndirectArgs
{	
	// Dispatch
	uint		threadGroupCountX;
	uint		threadGroupCountY;
	uint		threadGroupCountZ;
	// DrawIndexedInstanced
	uint		indexCountPerInstance;
	uint		instanceCount;
	uint		startIndexLocation;
	int			baseVertexLocation;
	uint		startInstanceLocation;	
	// Internal
	uint		blockCount;	
#if HLSL_DEBUG_FILL
	uint		conflictCount;
	uint		waitCount0;
	uint		waitCount1;
	uint		reuseCount;
#endif // #if HLSL_DEBUG_FILL
};

#define gridIndirectArgs_threadGroupCountX		gridIndirectArgs[0]
#define gridIndirectArgs_threadGroupCountY		gridIndirectArgs[1]
#define gridIndirectArgs_threadGroupCountZ		gridIndirectArgs[2]

#define gridIndirectArgs_indexCountPerInstance	gridIndirectArgs[3]
#define gridIndirectArgs_instanceCount			gridIndirectArgs[4]
#define gridIndirectArgs_startIndexLocation		gridIndirectArgs[5]
#define gridIndirectArgs_baseVertexLocation		gridIndirectArgs[6]
#define gridIndirectArgs_startInstanceLocation	gridIndirectArgs[7]

#define gridIndirectArgs_blockCount				gridIndirectArgs[8]
#if HLSL_DEBUG_FILL
#define gridIndirectArgs_conflictCount			gridIndirectArgs[9]
#define gridIndirectArgs_waitCount0				gridIndirectArgs[10]
#define gridIndirectArgs_waitCount1				gridIndirectArgs[11]
#define gridIndirectArgs_reuseCount				gridIndirectArgs[12]
#endif // #if HLSL_DEBUG_FILL

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__