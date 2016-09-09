/*V6*/

#ifndef __V6_HLSL_CAPTURE_SHARED_H__
#define __V6_HLSL_CAPTURE_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_DEBUG_COLLECT							0

#define HLSL_SAMPLE_THREAD_GROUP_SIZE				128
#define HLSL_OCTREE_THREAD_GROUP_SIZE				64
#define HLSL_BLOCK_THREAD_GROUP_SIZE				64

#define HLSL_NODE_CREATED							0x80000000
#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFFFFFFFF

// collect
#define HLSL_CAPTURE_COLOR_SLOT						0
#define HLSL_CAPTURE_DEPTH_SLOT						1

// collect/build/fill
#define HLSL_SAMPLE_SLOT							0
#define HLSL_SAMPLE_INDIRECT_ARGS_SLOT				1

// build/fill
#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT			2

// build/fill/pack
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT			3
#define HLSL_OCTREE_LEAF_SLOT						4

// build/pack
#define HLSL_OCTREE_INDIRECT_ARGS_SLOT				5

// pack
#define HLSL_BLOCK_POS_SLOT							0
#define HLSL_BLOCK_DATA_SLOT						1
#define HLSL_BLOCK_INDIRECT_ARGS_SLOT				2

CBUFFER( CBSample, 0 )
{
	float4				c_sampleRight;
	float4				c_sampleUp;
	float4				c_sampleForward;
	
	float				c_sampleDepthLinearScale;
	float				c_sampleDepthLinearBias;
	float				c_sampleInvCubeSize;
	uint				c_samplePad1;

	float3				c_sampleGridOrigin;
	uint				c_sampleGridWidth;
	
	float3				c_samplePos;
	uint				c_samplePad2;

	float3				c_sampleSkyboxMinRS;
	float				c_samplePad3;
	float3				c_sampleSkyboxMaxRS;
	uint				c_sampleMipSky;
	
	float4				c_sampleMipBoundaries[HLSL_MIP_MAX_COUNT];
	float4				c_sampleInvGridScales[HLSL_MIP_MAX_COUNT];
};

CBUFFER( CBOctree, 1 )
{
	uint				c_octreeCurrentLevel;
	uint				c_octreeLevelCount;
	uint				c_octreeCurrentBucket;
	uint				c_octreePad;
};


struct Sample
{
	uint row0;
	uint row1;
};

struct OctreeLeaf
{
	uint	mip4_none1_x9_y9_z9;
	uint	done1_x2y2z2_count25;
	uint	r32;
	uint	g32;
	uint	b32;
};

#define sample_groupCountX_offset							0
#define sample_groupCountY_offset							1
#define sample_groupCountZ_offset							2
#define sample_count_offset									3
#if HLSL_DEBUG_COLLECT == 1
#define sample_out_offset									4
#define sample_error_offset									5
#define sample_pixelCount_offset							6
#define sample_pixelSampleCount_offset						7
#define sample_all_offset									8
#else
#define sample_all_offset									4
#endif // #if HLSL_DEBUG_COLLECT == 1

#define sample_groupCountX									sampleIndirectArgs[sample_groupCountX_offset] // ThreadGroupCountX
#define sample_groupCountY									sampleIndirectArgs[sample_groupCountY_offset] // ThreadGroupCountY
#define sample_groupCountZ									sampleIndirectArgs[sample_groupCountZ_offset] // ThreadGroupCountZ
#if HLSL_DEBUG_COLLECT == 1
#define sample_out											sampleIndirectArgs[sample_out_offset]
#define sample_error										sampleIndirectArgs[sample_error_offset]
#define sample_pixelCount									sampleIndirectArgs[sample_pixelCount_offset]
#define sample_pixelSampleCount								sampleIndirectArgs[sample_pixelSampleCount_offset]
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

#define block_groupCountX_offset( BUCKET )					(BUCKET * 3 + 0)
#define block_groupCountY_offset( BUCKET )					(BUCKET * 3 + 1)
#define block_groupCountZ_offset( BUCKET )					(BUCKET * 3 + 2)

#define block_count_offset(	BUCKET )						(block_groupCountZ_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_posOffset_offset( BUCKET )					(block_count_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_dataOffset_offset( BUCKET )					(block_posOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_cellCount_offset( BUCKET )					(block_dataOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET)

#define block_uniqueOccupancyCount_offset( BUCKET )			(block_cellCount_offset( HLSL_BUCKET_COUNT ) + BUCKET)
#define block_uniqueOccupancyMax_offset( BUCKET )			(block_uniqueOccupancyCount_offset( HLSL_BUCKET_COUNT ) + BUCKET)
#define block_slotOccupancyCount_offset( BUCKET )			(block_uniqueOccupancyMax_offset( HLSL_BUCKET_COUNT ) + BUCKET)

#define block_all_offset									block_slotOccupancyCount_offset( HLSL_BUCKET_COUNT )

#define block_groupCountX( BUCKET )							blockIndirectArgs[block_groupCountX_offset( BUCKET )]
#define block_groupCountY( BUCKET )							blockIndirectArgs[block_groupCountY_offset( BUCKET )]
#define block_groupCountZ( BUCKET )							blockIndirectArgs[block_groupCountZ_offset( BUCKET )]

#define block_count( BUCKET )								blockIndirectArgs[block_count_offset( BUCKET )]
#define block_posOffset( BUCKET )							blockIndirectArgs[block_posOffset_offset( BUCKET )]
#define block_dataOffset( BUCKET )							blockIndirectArgs[block_dataOffset_offset( BUCKET )]
#define block_cellCount( BUCKET )							blockIndirectArgs[block_cellCount_offset( BUCKET )]

#define block_uniqueOccupancyCount( BUCKET )				blockIndirectArgs[block_uniqueOccupancyCount_offset( BUCKET )]
#define block_uniqueOccupancyMax( BUCKET )					blockIndirectArgs[block_uniqueOccupancyMax_offset( BUCKET )]
#define block_slotOccupancyCount( BUCKET )					blockIndirectArgs[block_slotOccupancyCount_offset( BUCKET )]

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_CAPTURE_SHARED_H__