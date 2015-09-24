/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define CONCAT( X, Y )								X ## Y
#define GROUP_COUNT( C, S )							(((C) + (S) - 1)) / (S)

#define HLSL_DEBUG_COLLECT							1
#define HLSL_DEBUG_PACK								1
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
#define HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT	3
#define HLSL_SORTED_SAMPLE_SLOT						4
#define HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SLOT		5
#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT			6
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT			7
#define HLSL_OCTREE_LEAF_SLOT						8
#define HLSL_OCTREE_INDIRECT_ARGS_SLOT				9
#define HLSL_PACKED_COLOR_SLOT						10
#define HLSL_PACKED_INDIRECT_ARGS_SLOT				11

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
#define HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SRV		CONCAT( t, HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT )
#define HLSL_SORTED_SAMPLE_SRV						CONCAT( t, HLSL_SORTED_SAMPLE_SLOT )
#define HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SRV		CONCAT( t, HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_SRV						CONCAT( t, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_PACKED_COLOR_SRV						CONCAT( t, HLSL_PACKED_COLOR_SLOT )
#define HLSL_PACKED_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_PACKED_INDIRECT_ARGS_SLOT )

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
#define HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_UAV		CONCAT( u, HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SLOT )
#define HLSL_SORTED_SAMPLE_UAV						CONCAT( u, HLSL_SORTED_SAMPLE_SLOT )
#define HLSL_SORTED_SAMPLE_INDIRECT_ARGS_UAV		CONCAT( u, HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_UAV						CONCAT( u, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_PACKED_COLOR_UAV						CONCAT( u, HLSL_PACKED_COLOR_SLOT )
#define HLSL_PACKED_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_PACKED_INDIRECT_ARGS_SLOT )

#define HLSL_GRID_THREAD_GROUP_SIZE					128

#define HLSL_GRID_BUCKET_COUNT						5

#define HLSL_GRID_MACRO_SHIFT						6
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
	float		c_sampleDepthLinearScale;
	float		c_sampleDepthLinearBias;
	float2		c_sampleInvCubeSize;
	float3		c_sampleCubeCenter;
	uint		c_sampleCurrentMip;
	float4		c_sampleMipBoundariesA;
	float4		c_sampleMipBoundariesB;
	float4		c_sampleMipBoundariesC;
	float4		c_sampleMipBoundariesD;
	float4		c_sampleInvGridScales[HLSL_MIP_MAX_COUNT];	
};

CBUFFER( CBOctree, 3 )
{
	uint		c_octreeCurrentMip;
	uint		c_octreeCurrentLevel;
	uint		c_octreeCurrentBucket;
	uint		c_octreePad;
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
	uint x8_r;
	uint y8_g;
	uint z8_b;
	uint x4y4z4_count;
};

#define gridIndirectArgs_packThreadGroupCount				gridIndirectArgs[0]
#define gridIndirectArgs_renderInstanceCount( BUCKET )		gridIndirectArgs[3 + 1 + (BUCKET) * 5]
#define gridIndirectArgs_blockCount							gridIndirectArgs[28]
#define gridIndirectArgs_packedBlockCounts( BUCKET )		gridIndirectArgs[29 + BUCKET]

#if HLSL_DEBUG_FILL
#define gridIndirectArgs_conflictCount						gridIndirectArgs[34]
#define gridIndirectArgs_waitCount0							gridIndirectArgs[35]
#define gridIndirectArgs_waitCount1							gridIndirectArgs[36]
#define gridIndirectArgs_reuseCount							gridIndirectArgs[37]
#endif // #if HLSL_DEBUG_FILL

#if HLSL_GRIDBLOCK_CELL_STATS
#define gridIndirectArgs_cellCount							gridIndirectArgs[38]
#endif

#define collectedSample_sortGroupCountX_offset				0
#define collectedSample_sortGroupCountY_offset				1
#define collectedSample_sortGroupCountZ_offset				2
#define collectedSample_count_offset						3
#if HLSL_DEBUG_COLLECT == 1
#define collectedSample_error_offset						4
#define collectedSample_all_offset							5
#else
#define collectedSample_all_offset							4
#endif // #if HLSL_DEBUG_COLLECT == 1

#define sample_sortGroupCountX								collectedSampleIndirectArgs[collectedSample_sortGroupCountX_offset] // ThreadGroupCountX
#define sample_sortGroupCountY								collectedSampleIndirectArgs[collectedSample_sortGroupCountY_offset] // ThreadGroupCountY
#define sample_sortGroupCountZ								collectedSampleIndirectArgs[collectedSample_sortGroupCountZ_offset] // ThreadGroupCountZ
#if HLSL_DEBUG_COLLECT == 1
#define sample_error										collectedSampleIndirectArgs[collectedSample_error_offset]
#endif // #if HLSL_DEBUG_COLLECT == 1
#define sample_count										collectedSampleIndirectArgs[collectedSample_count_offset]

#define sortedSample_groupCountX_offset( MIP )				(MIP*3 + 0)
#define sortedSample_groupCountY_offset( MIP )				(MIP*3 + 1)
#define sortedSample_groupCountZ_offset( MIP )				(MIP*3 + 2)
#define sortedSample_count_offset( MIP )					(HLSL_MIP_MAX_COUNT*3 + MIP + 1)
#define sortedSample_sum_offset( MIP )						(sortedSample_count_offset( HLSL_MIP_MAX_COUNT ) + MIP + 1)
#define sortedSample_all_offset								sortedSample_sum_offset( HLSL_MIP_MAX_COUNT )

#define sortedSample_groupCountX( MIP )						sortedSampleIndirectArgs[sortedSample_groupCountX_offset( MIP )] // ThreadGroupCountX
#define sortedSample_groupCountY( MIP )						sortedSampleIndirectArgs[sortedSample_groupCountY_offset( MIP )] // ThreadGroupCountY
#define sortedSample_groupCountZ( MIP )						sortedSampleIndirectArgs[sortedSample_groupCountZ_offset( MIP )] // ThreadGroupCountZ
#define sortedSample_count( MIP )							sortedSampleIndirectArgs[sortedSample_count_offset( MIP )]
#define sortedSample_sum( MIP )								sortedSampleIndirectArgs[sortedSample_sum_offset( MIP )]

#define octree_leafGroupCountX_offset( MIP )				(MIP*3 + 0)
#define octree_leafGroupCountY_offset( MIP )				(MIP*3 + 1)
#define octree_leafGroupCountZ_offset( MIP )				(MIP*3 + 2)
#define octree_leafCount_offset( MIP )						(HLSL_MIP_MAX_COUNT*3 + MIP + 1)
#define octree_leafSum_offset( MIP )						(octree_leafCount_offset( HLSL_MIP_MAX_COUNT ) + MIP + 1)
#define octree_nodeCount_offset								octree_leafSum_offset( HLSL_MIP_MAX_COUNT )
#define octree_all_offset									(octree_nodeCount_offset + 1)

#define octree_leafGroupCountX( MIP )						octreeIndirectArgs[octree_leafGroupCountX_offset( MIP )] // ThreadGroupCountX
#define octree_leafGroupCountY( MIP )						octreeIndirectArgs[octree_leafGroupCountY_offset( MIP )] // ThreadGroupCountY
#define octree_leafGroupCountZ( MIP )						octreeIndirectArgs[octree_leafGroupCountZ_offset( MIP )] // ThreadGroupCountZ
#define octree_leafCount( MIP )								octreeIndirectArgs[octree_leafCount_offset( MIP )]
#define octree_leafSum( MIP )								octreeIndirectArgs[octree_leafSum_offset( MIP )]
#define octree_nodeCount									octreeIndirectArgs[octree_nodeCount_offset]

#define packed_vertexCountPerInstance_offset( MIP, BUCKET )	((MIP*HLSL_BUCKET_COUNT + BUCKET) * 4 + 0)
#define packed_renderInstanceCount_offset( MIP, BUCKET )	((MIP*HLSL_BUCKET_COUNT + BUCKET) * 4 + 1)
#define packed_startVertexLocation_offset( MIP, BUCKET )	((MIP*HLSL_BUCKET_COUNT + BUCKET) * 4 + 2)
#define packed_renderInstanceLocation_offset( MIP, BUCKET )	((MIP*HLSL_BUCKET_COUNT + BUCKET) * 4 + 3)
#define packed_blockCount_offset( MIP, BUCKET )				(packed_renderInstanceLocation_offset( HLSL_MIP_MAX_COUNT, HLSL_BUCKET_COUNT ) + MIP*HLSL_BUCKET_COUNT + BUCKET + 1)
#define packed_blockSum_offset( MIP, BUCKET )				(packed_blockCount_offset( HLSL_MIP_MAX_COUNT, HLSL_BUCKET_COUNT ) + MIP*HLSL_BUCKET_COUNT + BUCKET + 1)
#define packed_cellCount_offset( MIP, BUCKET )				(packed_blockSum_offset( HLSL_MIP_MAX_COUNT, HLSL_BUCKET_COUNT ) + MIP*HLSL_BUCKET_COUNT + BUCKET)
#define packed_all_offset									packed_cellCount_offset( HLSL_MIP_MAX_COUNT, HLSL_BUCKET_COUNT )

#define packed_vertexCountPerInstance( MIP, BUCKET )		packedIndirectArgs[packed_vertexCountPerInstance_offset( MIP, BUCKET )]
#define packed_renderInstanceCount( MIP, BUCKET )			packedIndirectArgs[packed_renderInstanceCount_offset( MIP, BUCKET )]
#define packed_startVertexLocation( MIP, BUCKET )			packedIndirectArgs[packed_startVertexLocation_offset( MIP, BUCKET )]
#define packed_renderInstanceLocation( MIP, BUCKET )		packedIndirectArgs[packed_renderInstanceLocation_offset( MIP, BUCKET )]
#define packed_blockCount( MIP, BUCKET )					packedIndirectArgs[packed_blockCount_offset( MIP, BUCKET )]
#define packed_cellCount( MIP, BUCKET )						packedIndirectArgs[packed_cellCount_offset( MIP, BUCKET )]
#define packed_blockSum( MIP, BUCKET )						packedIndirectArgs[packed_blockSum_offset( MIP, BUCKET )]

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__