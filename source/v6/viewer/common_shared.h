/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_FIRST_SLOT					0
#define HLSL_COLOR_SLOT					0
#define HLSL_DEPTH_SLOT					1

#define HLSL_GRIDBLOCK_COLOR_SLOT			2
#define HLSL_GRIDBLOCK_ASSIGNED_ID_SLOT		3
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT	4
#define HLSL_GRIDBLOCK_PACKEDCOLOR_SLOT		5


#define CONCAT( X, Y )						X ## Y

#define HLSL_COLOR_SRV						CONCAT( t, HLSL_COLOR_SLOT )
#define HLSL_DEPTH_SRV						CONCAT( t, HLSL_DEPTH_SLOT )
#define HLSL_GRIDBLOCK_COLOR_SRV			CONCAT( t, HLSL_GRIDBLOCK_COLOR_SLOT )
#define HLSL_GRIDBLOCK_ASSIGNED_ID_SRV		CONCAT( t, HLSL_GRIDBLOCK_ASSIGNED_ID_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR_SRV		CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR_SLOT )

#define HLSL_GRIDBLOCK_COLOR_UAV			CONCAT( u, HLSL_GRIDBLOCK_COLOR_SLOT )
#define HLSL_GRIDBLOCK_ASSIGNED_ID_UAV		CONCAT( u, HLSL_GRIDBLOCK_ASSIGNED_ID_SLOT )
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV	CONCAT( u, HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR_UAV		CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR_SLOT )

#define HLSL_GRID_CLEAR_GROUP_SIZE			128
#define HLSL_GRID_PACK_GROUP_SIZE			128

#define HLSL_GRID_MACRO_SHIFT				5
#define HLSL_GRID_MACRO_2XSHIFT				(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT				(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_WIDTH				(1 << HLSL_GRID_MACRO_SHIFT)

#define HLSL_GRID_BLOCK_COUNT				(1 << HLSL_GRID_MACRO_3XSHIFT)
#define HLSL_GRID_BLOCK_SHIFT				2
#define HLSL_GRID_BLOCK_2XSHIFT				(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_3XSHIFT				(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_WIDTH				(1 << HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_MASK				(HLSL_GRID_BLOCK_WIDTH-1)
#define HLSL_GRID_BLOCK_INVALID				uint( -1 )

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

struct GridColor
{
	uint r;
	uint g;
	uint b;
	uint a;
};

struct GridPackedColor
{
	uint rgba;
};

struct GridBlockColor
{
	GridColor colors[HLSL_GRID_BLOCK_CELL_COUNT];
};

struct GridBlockPackedColor
{
	GridPackedColor		colors[HLSL_GRID_BLOCK_CELL_COUNT];
	uint				blockPos;
};

struct GridIndirectArgs
{
  uint	indexCountPerInstance;
  uint	instanceCount;
  uint	startIndexLocation;
  int	baseVertexLocation;
  uint	sartInstanceLocation;
  uint	blockCount;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__