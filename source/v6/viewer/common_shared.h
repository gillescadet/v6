/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define CONCAT( X, Y )								X ## Y
#define GROUP_COUNT( C, S )							(((C) + (S) - 1)) / (S)

#define HLSL_DEBUG_COLLECT							1
#define HLSL_DEBUG_BLOCK							0

#define HLSL_COLOR_SLOT								0
#define HLSL_DEPTH_SLOT								1

#define HLSL_SAMPLE_SLOT							2
#define HLSL_SAMPLE_INDIRECT_ARGS_SLOT				3
#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT			6
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT			7
#define HLSL_OCTREE_LEAF_SLOT						8
#define HLSL_OCTREE_INDIRECT_ARGS_SLOT				9
#define HLSL_BLOCK_COLOR_SLOT						10
#define HLSL_BLOCK_INDIRECT_ARGS_SLOT				11

#define HLSL_COLOR_SRV								CONCAT( t, HLSL_COLOR_SLOT )
#define HLSL_DEPTH_SRV								CONCAT( t, HLSL_DEPTH_SLOT )

#define HLSL_SAMPLE_SRV								CONCAT( t, HLSL_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_SRV						CONCAT( t, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_BLOCK_COLOR_SRV						CONCAT( t, HLSL_BLOCK_COLOR_SLOT )
#define HLSL_BLOCK_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_BLOCK_INDIRECT_ARGS_SLOT )

#define HLSL_SAMPLE_UAV								CONCAT( u, HLSL_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_UAV						CONCAT( u, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_BLOCK_COLOR_UAV						CONCAT( u, HLSL_BLOCK_COLOR_SLOT )
#define HLSL_BLOCK_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_BLOCK_INDIRECT_ARGS_SLOT )

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
#define HLSL_BUCKET_COUNT							5

CBUFFER( CBView, 0 )
{
	row_major	matrix	c_frameObjectToView;
	row_major	matrix	c_frameViewToProj;
	uint				c_frameWidth;
	uint				c_frameHeight;
	int					c_framePad1;
	int					c_framePad2;
};

CBUFFER( CBSample, 1 )
{
	float				c_sampleDepthLinearScale;
	float				c_sampleDepthLinearBias;
	float2				c_sampleInvCubeSize;
	float3				c_sampleCubeCenter;
	float				c_samplePad0;
	float4				c_sampleMipBoundariesA;
	float4				c_sampleMipBoundariesB;
	float4				c_sampleMipBoundariesC;
	float4				c_sampleMipBoundariesD;
	float4				c_sampleInvGridScales[HLSL_MIP_MAX_COUNT];	
};

CBUFFER( CBOctree, 2 )
{
	uint				c_octreeCurrentLevel;
	uint				c_octreeCurrentBucket;
	float				c_octreePad0;
	float				c_octreePad1;
};

CBUFFER( CBBlock, 3 )
{
	float4				c_blockGridScales[HLSL_MIP_MAX_COUNT];
};

struct Sample
{
	uint row0;
	uint row1;
};

struct OctreeLeaf
{
	uint x8_r24;
	uint y8_g24;
	uint z8_b24;
	uint x4y4z4_mip4_count16;
};

#define sample_groupCountX_offset							0
#define sample_groupCountY_offset							1
#define sample_groupCountZ_offset							2
#define sample_count_offset									3
#if HLSL_DEBUG_COLLECT == 1
#define sample_error_offset									4
#define sample_all_offset									5
#else
#define sample_all_offset									4
#endif // #if HLSL_DEBUG_COLLECT == 1

#define sample_groupCountX									sampleIndirectArgs[sample_groupCountX_offset] // ThreadGroupCountX
#define sample_groupCountY									sampleIndirectArgs[sample_groupCountY_offset] // ThreadGroupCountY
#define sample_groupCountZ									sampleIndirectArgs[sample_groupCountZ_offset] // ThreadGroupCountZ
#if HLSL_DEBUG_COLLECT == 1
#define sample_error										sampleIndirectArgs[sample_error_offset]
#endif // #if HLSL_DEBUG_COLLECT == 1
#define sample_count										sampleIndirectArgs[sample_count_offset]

#define octree_leafGroupCountX_offset						0
#define octree_leafGroupCountY_offset						1
#define octree_leafGroupCountZ_offset						2
#define octree_leafCount_offset								3
#define octree_nodeCount_offset								4
#define octree_all_offset									5

#define octree_leafGroupCountX								octreeIndirectArgs[octree_leafGroupCountX_offset] // ThreadGroupCountX
#define octree_leafGroupCountY								octreeIndirectArgs[octree_leafGroupCountY_offset] // ThreadGroupCountY
#define octree_leafGroupCountZ								octreeIndirectArgs[octree_leafGroupCountZ_offset] // ThreadGroupCountZ
#define octree_leafCount									octreeIndirectArgs[octree_leafCount_offset]
#define octree_nodeCount									octreeIndirectArgs[octree_nodeCount_offset]

#define block_vertexCountPerInstance_offset( BUCKET )		(BUCKET * 4 + 0)
#define block_renderInstanceCount_offset( BUCKET )			(BUCKET * 4 + 1)
#define block_startVertexLocation_offset( BUCKET )			(BUCKET * 4 + 2)
#define block_renderInstanceLocation_offset( BUCKET )		(BUCKET * 4 + 3)
#define block_count_offset(	BUCKET )						(block_renderInstanceLocation_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_packedOffset_offset( BUCKET )					(block_count_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_cellCount_offset( BUCKET )					(block_packedOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET)
#define block_all_offset									block_cellCount_offset( HLSL_BUCKET_COUNT )

#define block_vertexCountPerInstance( BUCKET )				blockIndirectArgs[block_vertexCountPerInstance_offset( BUCKET )]
#define block_renderInstanceCount( BUCKET )					blockIndirectArgs[block_renderInstanceCount_offset( BUCKET )]
#define block_startVertexLocation( BUCKET )					blockIndirectArgs[block_startVertexLocation_offset( BUCKET )]
#define block_renderInstanceLocation( BUCKET )				blockIndirectArgs[block_renderInstanceLocation_offset( BUCKET )]
#define block_count( BUCKET )								blockIndirectArgs[block_count_offset( BUCKET )]
#define block_packedOffset( BUCKET )						blockIndirectArgs[block_packedOffset_offset( BUCKET )]
#define block_cellCount( BUCKET )							blockIndirectArgs[block_cellCount_offset( BUCKET )]

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__