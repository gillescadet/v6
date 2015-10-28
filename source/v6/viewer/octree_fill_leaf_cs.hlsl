#define HLSL
#include "common_shared.h"
#include "sample_pack.hlsli"

StructuredBuffer< Sample > samples				: register( HLSL_SAMPLE_SRV );
Buffer< uint > sampleIndirectArgs 				: register( HLSL_SAMPLE_INDIRECT_ARGS_SRV );
Buffer< uint > sampleNodeOffsets 				: register( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SRV );
Buffer< uint > firstChildOffsets				: register( HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV );

RWStructuredBuffer< OctreeLeaf > octreeLeaves	: register( HLSL_OCTREE_LEAF_UAV );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint sampleID = DTid.x;
	
	if ( sampleID >= sample_count )
		return;

	uint3 color;
	uint occupancy;
	Sample_UnpackColorAndOccupancy( samples[sampleID], color, occupancy );

	const uint nodeOffset = sampleNodeOffsets[sampleID];
	const uint leafID = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
	InterlockedAdd( octreeLeaves[leafID].x9_r23, color.r );
	InterlockedAdd( octreeLeaves[leafID].y9_g23, color.g );
	InterlockedAdd( octreeLeaves[leafID].z9_b23, color.b );
	InterlockedAdd( octreeLeaves[leafID].x2y2z2_mip4_count15, 1 );
	InterlockedOr( octreeLeaves[leafID].occupancy27, occupancy );
}
