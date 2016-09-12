#define HLSL

#include "../graphic/capture_shared.h"
#include "../graphic/sample_pack.hlsli"

StructuredBuffer< Sample > samples					: REGISTER_SRV( HLSL_SAMPLE_SLOT );
Buffer< uint > sampleIndirectArgs 					: REGISTER_SRV( HLSL_SAMPLE_INDIRECT_ARGS_SLOT );
Buffer< uint > sampleNodeOffsets 					: REGISTER_SRV( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT );
Buffer< uint > firstChildOffsets					: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT );

#if ONION == 1
RWStructuredBuffer< OctreeLeafOnion > octreeLeaves	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT );
#else
RWStructuredBuffer< OctreeLeaf > octreeLeaves		: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT );
#endif

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint sampleID = DTid.x;
	
	if ( sampleID >= sample_count )
		return;

	uint3 color;
#if ONION == 1
	SampleOnion_UnpackColor( samples[sampleID], color );
#else
	Sample_UnpackColor( samples[sampleID], color );
#endif

	const uint nodeOffset = sampleNodeOffsets[sampleID];
	const uint leafID = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
	
	uint prevCount;
	InterlockedAdd( octreeLeaves[leafID].done1_x2y2z2_count25, 1, prevCount );
	if ( (prevCount & 0x1FFFFFF) < 0xFFFFFF )
	{
		InterlockedAdd( octreeLeaves[leafID].r32, color.r * 1 );
		InterlockedAdd( octreeLeaves[leafID].g32, color.g * 1 );
		InterlockedAdd( octreeLeaves[leafID].b32, color.b * 1 );
	}
	else
	{
		InterlockedAdd( (int)octreeLeaves[leafID].done1_x2y2z2_count25, -1 );
	}
}
