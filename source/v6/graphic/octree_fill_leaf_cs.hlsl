#define HLSL

#include "../graphic/capture_shared.h"
#include "../graphic/sample_pack.hlsli"

StructuredBuffer< Sample > samples				: REGISTER_SRV( HLSL_SAMPLE_SLOT );
Buffer< uint > sampleIndirectArgs 				: REGISTER_SRV( HLSL_SAMPLE_INDIRECT_ARGS_SLOT );
Buffer< uint > sampleNodeOffsets 				: REGISTER_SRV( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT );
Buffer< uint > firstChildOffsets				: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT );

RWStructuredBuffer< OctreeLeaf > octreeLeaves	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main_octree_fill_leaf_cs( uint3 DTid : SV_DispatchThreadID )
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
