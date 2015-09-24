#define HLSL
#include "common_shared.h"
#include "sample_pack.hlsli"

StructuredBuffer< Sample > samples				: register( HLSL_SORTED_SAMPLE_SRV );
Buffer< uint > sortedSampleIndirectArgs 		: register( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SRV );
Buffer< uint > sampleNodeOffsets 				: register( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SRV );
Buffer< uint > firstChildOffsets				: register( HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV );

RWStructuredBuffer< OctreeLeaf > octreeLeaves	: register( HLSL_OCTREE_LEAF_UAV );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint sampleID = DTid.x;
	
	if ( sampleID >= sortedSample_count( c_octreeCurrentMip ) )
		return;

	sampleID += sortedSample_sum( c_octreeCurrentMip );

	const uint3 color = Sample_UnpackColor( samples[sampleID] );

	const uint nodeOffset = sampleNodeOffsets[sampleID];
	const uint leafID = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
	InterlockedAdd( octreeLeaves[leafID].x8_r, color.r );
	InterlockedAdd( octreeLeaves[leafID].y8_g, color.g );
	InterlockedAdd( octreeLeaves[leafID].z8_b, color.b );
	InterlockedAdd( octreeLeaves[leafID].x4y4z4_count, 1 );	
}
