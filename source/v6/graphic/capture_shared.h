/*V6*/

#ifndef __V6_HLSL_CAPTURE_SHARED_H__
#define __V6_HLSL_CAPTURE_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_DEBUG_COLLECT									0

#define HLSL_OCTREE_LEAF_SHIFT								24u
#define HLSL_OCTREE_LEAF_MAX_COUNT_PER_PAGE					(1u << HLSL_OCTREE_LEAF_SHIFT) // <= 512MB to work around a (nVidia?) limitation
#define HLSL_OCTREE_LEAF_MOD								(HLSL_OCTREE_LEAF_MAX_COUNT_PER_PAGE-1)
#define HLSL_OCTREE_LEAF_PAGE_MAX_COUNT						4u

#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SHIFT				26u
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_MAX_COUNT_PER_PAGE	(1u << HLSL_OCTREE_FIRST_CHILD_OFFSET_SHIFT) // <= 512MB to work around a (nVidia?) limitation
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_MOD					(HLSL_OCTREE_FIRST_CHILD_OFFSET_MAX_COUNT_PER_PAGE-1)
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_PAGE_MAX_COUNT		2u

#define HLSL_SAMPLE_THREAD_GROUP_SIZEX						8u
#define HLSL_SAMPLE_THREAD_GROUP_SIZEY						8u
#define HLSL_SAMPLE_THREAD_MAX_COUNTX						(HLSL_SAMPLE_THREAD_GROUP_SIZEX * 4096u)

#define HLSL_OCTREE_THREAD_GROUP_SIZEX						8u
#define HLSL_OCTREE_THREAD_GROUP_SIZEY						8u
#define HLSL_OCTREE_THREAD_MAX_COUNTX						(HLSL_OCTREE_THREAD_GROUP_SIZEX * 4096u)

#define HLSL_NODE_CREATED									0x80000000
#define HLSL_GRID_BLOCK_CELL_EMPTY							0xFFFFFFFF

// collect
#define HLSL_CAPTURE_COLOR_SLOT								0
#define HLSL_CAPTURE_DEPTH_SLOT								1

// collect UAV, build/fill SRV
#define HLSL_SAMPLE_SLOT									0
#define HLSL_SAMPLE_INFO_SLOT								1

// collect UAV
#define HLSL_SAMPLE_DEBUG_SLOT								3

// build/fill UAV, pack SRV
#define HLSL_OCTREE_LEAF_SLOT0								0
#define HLSL_OCTREE_LEAF_SLOT1								1
#define HLSL_OCTREE_LEAF_SLOT2								2
#define HLSL_OCTREE_LEAF_SLOT3								3

// build UAV, fill SRV
#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT					4

// build UAV, fill/pack SRV
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT0				5
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT1				6

// build UAV, pack SRV
#define HLSL_OCTREE_INFO_SLOT								7

// pack UAV
#define HLSL_BLOCK_POS_SLOT									0
#define HLSL_BLOCK_DATA_SLOT								1
#define HLSL_BLOCK_INFO_SLOT								2

CBUFFER( CBSample, 0 )
{
	float4				c_sampleRight;
	float4				c_sampleUp;
	float4				c_sampleForward;
	
	float				c_sampleDepthLinearScale;
	float				c_sampleDepthLinearBias;
	float				c_sampleInvCubeSize;
	float				c_sampleGammaCorrection;

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

CBUFFER( CBSampleOnion, 1 )
{
	float				c_sampleOnionDepthLinearScale;
	float				c_sampleOnionDepthLinearBias;
	float				c_sampleOnionInvSamplingWidth;
	float				c_sampleOnionGammaCorrection;

	float4				c_sampleOnionRight;
	float4				c_sampleOnionUp;
	float4				c_sampleOnionForward;

	float4				c_sampleOnionSampleCenterWS;
	float4				c_sampleOnionGridCenterWS;

	float4				c_sampleOnionSkyboxMinRS;
	float4				c_sampleOnionSkyboxMaxRS;
	
	float				c_sampleOnionGridMinScale;
	float				c_sampleOnionGridMaxScale;
	float				c_sampleOnionInvGridMinScale;
	float				c_sampleOnionPad2;

	float				c_sampleOnionMacroPeriodWidth;
	float				c_sampleOnionInvMacroPeriodWidth;
	float2				c_sampleOnionPad3;

	float				c_sampleOnionMacroGridWidth;
	float				c_sampleOnionHalfMacroGridWidth;
	float				c_sampleOnionInvMacroGridWidth;
	float				c_sampleOnionPad4;
};

CBUFFER( CBOctree, 2 )
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
	uint	count32;
	uint	r32;
	uint	g32;
	uint	b32;
};

struct OctreeLeafOnion
{
	uint	face3_x9_y9_z11;
	uint	count32;
	uint	r32;
	uint	g32;
	uint	b32;
};

struct SampleDebugOnion
{
	uint3	minBlockCoords;
	uint3	maxBlockCoords;
	uint	assertFailedBits;
	uint4	assertDataU32[4];
	float4	assertDataF32[4];
};

struct SampleInfo
{
	uint count;
#if HLSL_DEBUG_COLLECT == 1
	uint out;
	uint error;
	uint pixelCount;
	uint pixelSampleCount;
#endif // #if HLSL_DEBUG_COLLECT == 1
};

struct OctreeInfo
{
	uint leafCount;
	uint nodeCount;
	uint sampleInputCount;
	uint samplePassedCount;
};

struct BlockInfo
{
	uint counts[HLSL_BUCKET_COUNT + 1];
	uint posOffsets[HLSL_BUCKET_COUNT + 1];
	uint dataOffsets[HLSL_BUCKET_COUNT + 1];
	uint cellCounts[HLSL_BUCKET_COUNT];
	uint minNullLeafID;
	uint maxNullLeafID;
	uint minOverLeafID;
	uint maxOverLeafID;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_CAPTURE_SHARED_H__