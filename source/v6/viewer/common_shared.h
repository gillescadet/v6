/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define CONCAT( X, Y )								X ## Y
#define GROUP_COUNT( C, S )							(((C) + (S) - 1)) / (S)

#define HLSL_DEBUG_FILL								1
#define HLSL_GRIDBLOCK_CELL_STATS					1
#define HLSL_GRIDBLOCK_USE_POINTS					0

#define HLSL_FIRST_SLOT								0
#define HLSL_COLOR_SLOT								0
#define HLSL_DEPTH_SLOT								1

#define HLSL_GRIDBLOCK_ID_SLOT						2
#define HLSL_GRIDBLOCK_COLOR_SLOT					3
#define HLSL_GRIDBLOCK_POS_SLOT						4
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT			5
#define HLSL_GRIDBLOCK_PACKEDCOLOR4_SLOT			6
#define HLSL_GRIDBLOCK_PACKEDCOLOR8_SLOT			7
#define HLSL_GRIDBLOCK_PACKEDCOLOR16_SLOT			8
#define HLSL_GRIDBLOCK_PACKEDCOLOR32_SLOT			9
#define HLSL_GRIDBLOCK_PACKEDCOLOR64_SLOT			10

#define HLSL_COLLECTED_SAMPLE_SLOT					2
#define HLSL_SORTED_SAMPLE_SLOT						3
#define HLSL_SAMPLE_INDIRECT_ARGS_SLOT				4
#define HLSL_SAMPLE_NODE_OFFSET_SLOT				5
#define HLSL_FIRST_CHILD_OFFSET_SLOT				6
#define HLSL_OCTREE_LEAF_SLOT						7
#define HLSL_GRID_PACKEDCOLOR_SLOT					8

#define HLSL_COLOR_SRV								CONCAT( t, HLSL_COLOR_SLOT )
#define HLSL_DEPTH_SRV								CONCAT( t, HLSL_DEPTH_SLOT )

#define HLSL_GRIDBLOCK_ID_SRV						CONCAT( t, HLSL_GRIDBLOCK_ID_SLOT )
#define HLSL_GRIDBLOCK_COLOR_SRV					CONCAT( t, HLSL_GRIDBLOCK_COLOR_SLOT )
#define HLSL_GRIDBLOCK_POS_SRV						CONCAT( t, HLSL_GRIDBLOCK_POS_SLOT )
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_SRV			CONCAT( t, HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR4_SRV				CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR4_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR8_SRV				CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR8_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR16_SRV			CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR16_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR32_SRV			CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR32_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR64_SRV			CONCAT( t, HLSL_GRIDBLOCK_PACKEDCOLOR64_SLOT )

#define HLSL_COLLECTED_SAMPLE_SRV					CONCAT( t, HLSL_COLLECTED_SAMPLE_SLOT )
#define HLSL_SORTED_SAMPLE_SRV						CONCAT( t, HLSL_SORTED_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )
#define HLSL_SAMPLE_NODE_OFFSET_SRV					CONCAT( t, HLSL_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_FIRST_CHILD_OFFSET_SRV					CONCAT( t, HLSL_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_SRV						CONCAT( t, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_GRID_PACKEDCOLOR_SRV					CONCAT( t, HLSL_GRID_PACKEDCOLOR_SLOT )

#define HLSL_GRIDBLOCK_ID_UAV						CONCAT( u, HLSL_GRIDBLOCK_ID_SLOT )
#define HLSL_GRIDBLOCK_COLOR_UAV					CONCAT( u, HLSL_GRIDBLOCK_COLOR_SLOT )
#define HLSL_GRIDBLOCK_POS_UAV						CONCAT( u, HLSL_GRIDBLOCK_POS_SLOT )
#define HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV			CONCAT( u, HLSL_GRIDBLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR4_UAV				CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR4_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR8_UAV				CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR8_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR16_UAV			CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR16_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR32_UAV			CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR32_SLOT )
#define HLSL_GRIDBLOCK_PACKEDCOLOR64_UAV			CONCAT( u, HLSL_GRIDBLOCK_PACKEDCOLOR64_SLOT )

#define HLSL_COLLECTED_SAMPLE_UAV					CONCAT( u, HLSL_COLLECTED_SAMPLE_SLOT )
#define HLSL_SORTED_SAMPLE_UAV						CONCAT( u, HLSL_SORTED_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )
#define HLSL_SAMPLE_NODE_OFFSET_UAV					CONCAT( u, HLSL_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_FIRST_CHILD_OFFSET_UAV					CONCAT( u, HLSL_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_UAV						CONCAT( u, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_GRID_PACKEDCOLOR_UAV					CONCAT( u, HLSL_GRID_PACKEDCOLOR_SLOT )

#define HLSL_GRID_THREAD_GROUP_SIZE					128

#define HLSL_GRID_BUCKET_COUNT						5

#define HLSL_GRID_MACRO_SHIFT						9
#define HLSL_GRID_MACRO_2XSHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)w
#define HLSL_GRID_MACRO_WIDTH						(1 << HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_MASK						(HLSL_GRID_MACRO_WIDTH-1)

#define HLSL_GRID_BLOCK_COUNT						(1 << HLSL_GRID_MACRO_3XSHIFT)
#define HLSL_GRID_BLOCK_SHIFT						2
#define HLSL_GRID_BLOCK_2XSHIFT						(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_3XSHIFT						(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_WIDTH						(1 << HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_MASK						(HLSL_GRID_BLOCK_WIDTH-1)
#define HLSL_GRID_BLOCK_INVALID						uint( -1 )
#define HLSL_GRID_BLOCK_SETTING						uint( -2 )

#define HLSL_GRID_BLOCK_CELL_COUNT					(1 << HLSL_GRID_BLOCK_3XSHIFT)
#define HLSL_GRID_BLOCK_CELL_POS_MASK				(HLSL_GRID_BLOCK_CELL_COUNT-1)
#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFF

#define HLSL_GRID_SHIFT								(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_WIDTH								(1 << HLSL_GRID_SHIFT)
#define HLSL_GRID_MASK								(HLSL_GRID_WIDTH-1)
#define HLSL_GRID_INV_WIDTH							(1.0 / HLSL_GRID_WIDTH)
#define HLSL_GRID_HALF_WIDTH						(HLSL_GRID_WIDTH >> 1)
#define HLSL_GRID_QUARTER_WIDTH						(HLSL_GRID_WIDTH >> 2)

#define HLSL_SAMPLE_THREAD_GROUP_SIZE				128
#define HLSL_OCTREE_THREAD_GROUP_SIZE				128
#define	HLSL_MIP_MAX_COUNT							16
#define HLSL_NODE_CREATED							0x80000000

CBUFFER( CBView, 0 )
{
	row_major	matrix objectToView;
	row_major	matrix viewToProj;
	uint		frameWidth;
	uint		frameHeight;
	int			CBView_pad1;
	int			CBView_pad2;
};

#if 0

CBUFFER( CBGrid, 1 )
{
	float		depthLinearScale;
	float		depthLinearBias;
	float		invFrameSize;	
	float		gridScale;
	float3		offset;
	float		invGridScale;	
};

#endif

CBUFFER( CBSample, 2 )
{
	float		c_depthLinearScale;
	float		c_depthLinearBias;
	float2		c_invCubeSize;
	float3		c_cubeCenter;
	uint		c_currentMip;
	float4		c_mipBoundariesA;
	float4		c_mipBoundariesB;
	float4		c_mipBoundariesC;
	float4		c_mipBoundariesD;
	float		c_invGridScales[HLSL_MIP_MAX_COUNT];	
};

CBUFFER( CBOctree, 3 )
{
	uint		currentLevel;
	uint		currentBucket;
};

struct GridBlockColor
{
	uint4	colors[HLSL_GRID_BLOCK_CELL_COUNT];
	//uint	coverages[HLSL_GRID_BLOCK_CELL_COUNT];
};

struct GridPackIndirectArgs
{
	// Dispatch
	uint		threadGroupCountX;
	uint		threadGroupCountY;
	uint		threadGroupCountZ;
};

struct GridRenderIndrectArgs
{
#if HLSL_GRIDBLOCK_USE_POINTS
	uint		vertexCountPerInstance;
	uint		instanceCount;
	uint		startVertexLocation;
	uint		startInstanceLocation;
	uint		_pad;
#else
	// DrawIndexedInstanced
	uint		indexCountPerInstance;
	uint		instanceCount;
	uint		startIndexLocation;
	int			baseVertexLocation;
	uint		startInstanceLocation;	
#endif
};

struct GridIndirectArgs
{	
	GridPackIndirectArgs	packArgs;
	GridRenderIndrectArgs	renderArgs[HLSL_GRID_BUCKET_COUNT];
	
	// Internal
	uint		blockCount;
	uint		packedBlockCounts[HLSL_GRID_BUCKET_COUNT];
#if HLSL_DEBUG_FILL
	uint		conflictCount;
	uint		waitCount0;
	uint		waitCount1;
	uint		reuseCount;
#endif // #if HLSL_DEBUG_FILL
#if HLSL_GRIDBLOCK_CELL_STATS
	uint		cellCount;
#endif
};

struct Sample
{
	uint row0;
	uint row1;
};

struct OctreeLeaf
{
	uint r;
	uint g;
	uint b;
	uint x_y;
	uint z_count;
};

#define gridIndirectArgs_packThreadGroupCount			gridIndirectArgs[0]
#define gridIndirectArgs_renderInstanceCount( BUCKET )	gridIndirectArgs[3 + 1 + (BUCKET) * 5]
#define gridIndirectArgs_blockCount						gridIndirectArgs[28]
#define gridIndirectArgs_packedBlockCounts( BUCKET )	gridIndirectArgs[29 + BUCKET]

#if HLSL_DEBUG_FILL
#define gridIndirectArgs_conflictCount					gridIndirectArgs[34]
#define gridIndirectArgs_waitCount0						gridIndirectArgs[35]
#define gridIndirectArgs_waitCount1						gridIndirectArgs[36]
#define gridIndirectArgs_reuseCount						gridIndirectArgs[37]
#endif // #if HLSL_DEBUG_FILL

#if HLSL_GRIDBLOCK_CELL_STATS
#define gridIndirectArgs_cellCount						gridIndirectArgs[38]
#endif

#define sample_sortGroupCountX							sampleIndirectArgs[0] // ThreadGroupCountX
#define sample_sortGroupCountY							sampleIndirectArgs[1] // ThreadGroupCountY
#define sample_sortGroupCountZ							sampleIndirectArgs[2] // ThreadGroupCountZ
#define sample_count									sampleIndirectArgs[3]
#define	sample_cellPerLevelCount( MIP )					sampleIndirectArgs[4+MIP]

#define octree_nodeCount								sampleIndirectArgs[0]
#define octree_leaftCount								sampleIndirectArgs[1]
#define octree_blockCount(ID)							sampleIndirectArgs[ID]
#define octree_blockSum(ID)								sampleIndirectArgs[ID]

#define grid_renderInstanceCount(ID)					sampleIndirectArgs[ID]

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__