#define HLSL

#include "capture_shared.h"
#include "sample_pack.hlsli"

#if ONION == 1
#define OCTREE_LEAF OctreeLeafOnion
#else
#define OCTREE_LEAF OctreeLeaf
#endif

StructuredBuffer< Sample > samples					: REGISTER_SRV( HLSL_SAMPLE_SLOT );
StructuredBuffer< SampleInfo > sampleInfo			: REGISTER_SRV( HLSL_SAMPLE_INFO_SLOT );
Buffer< uint > sampleNodeOffsets 					: REGISTER_SRV( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT );
Buffer< uint > firstChildOffsets0					: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT0 );
Buffer< uint > firstChildOffsets1					: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT1 );

RWStructuredBuffer< OCTREE_LEAF > octreeLeaves0	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT0 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves1	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT1 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves2	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT2 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves3	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT3 );

#define OCTREE_LEAF_IS_READONLY					0
#define OCTREE_FIRST_CHILD_OFFSET_IS_READONLY	1
#include "octree_helpers.hlsli"

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZEX, HLSL_SAMPLE_THREAD_GROUP_SIZEY, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint sampleID = mad( DTid.y, HLSL_SAMPLE_THREAD_MAX_COUNTX, DTid.x );
	if ( sampleID >= sampleInfo[0].count )
		return;

	uint3 color;
#if ONION == 1
	SampleOnion_UnpackColor( samples[sampleID], color );
#else
	Sample_UnpackColor( samples[sampleID], color );
#endif

	const uint nodeOffset = sampleNodeOffsets[sampleID];
	const uint leafID = ReadOctreeFirstChildOffset( nodeOffset ) & ~HLSL_NODE_CREATED;

	UpdateOctreeLeaf( leafID, color );
}
