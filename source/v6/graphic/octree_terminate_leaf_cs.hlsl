#define HLSL
#include "../graphic/capture_shared.h"

Buffer< uint > octreeIndirectArgs 				: REGISTER_SRV( HLSL_OCTREE_INDIRECT_ARGS_SLOT );

RWStructuredBuffer< OctreeLeaf > octreeLeaves	: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT );

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main_octree_terminate_leaf_cs( uint3 DTid : SV_DispatchThreadID )
{
	const uint leafID = DTid.x;
	if ( leafID >= octree_leafCount )
		return;
		
	uint3 coords;
	if ( (octreeLeaves[leafID].done1_x2y2z2_count25 >> 31) != 0 )
		octreeLeaves[leafID].done1_x2y2z2_count25 |= 1 << 31;
}
