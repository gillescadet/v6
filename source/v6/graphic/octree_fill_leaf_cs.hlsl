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
	Sample_UnpackColor( samples[sampleID], color );

	const uint nodeOffset = sampleNodeOffsets[sampleID];
	const uint leafID = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
	
	// if ( (octreeLeaves[leafID].done1_x2y2z2_count25 >> 31) == 0 )
	{
		uint prevCount;
		InterlockedAdd( octreeLeaves[leafID].done1_x2y2z2_count25, c_octreeSampleWeight, prevCount );
		if ( (prevCount & 0x1FFFFFF) < 0xFFFFFF )
		{
			InterlockedAdd( octreeLeaves[leafID].r32, color.r * c_octreeSampleWeight );
			InterlockedAdd( octreeLeaves[leafID].g32, color.g * c_octreeSampleWeight );
			InterlockedAdd( octreeLeaves[leafID].b32, color.b * c_octreeSampleWeight );
		}
		else
		{
			InterlockedAdd( (int)octreeLeaves[leafID].done1_x2y2z2_count25, -(int)(c_octreeSampleWeight) );
		}
	}
}
