#define HLSL

#include "capture_shared.h"
#include "sample_pack.hlsli"

StructuredBuffer< Sample > samples					: REGISTER_SRV( HLSL_SAMPLE_SLOT );
Buffer< uint > sampleIndirectArgs 					: REGISTER_SRV( HLSL_SAMPLE_INDIRECT_ARGS_SLOT );

RWBuffer< uint > sampleNodeOffsets 					: REGISTER_UAV( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT );
RWBuffer< uint > firstChildOffsets					: REGISTER_UAV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT );
#if ONION == 1
RWStructuredBuffer< OctreeLeafOnion > octreeLeaves	: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT );
#else
RWStructuredBuffer< OctreeLeaf > octreeLeaves		: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT );
#endif
RWBuffer< uint > octreeIndirectArgs 				: REGISTER_UAV( HLSL_OCTREE_INDIRECT_ARGS_SLOT );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint sampleID = DTid.x;
		
#if BUILD_INNER == 0
	if ( DTid.x == 0 )
	{
		InterlockedMax( octree_leafGroupCountX, uint( 1 ) );
		octree_leafGroupCountY = 1;
		octree_leafGroupCountZ = 1;
	}
#endif // #if BUILD_INNER == 0

	if ( sampleID >= sample_count )
		return;

	uint3 coords;
#if ONION
	uint face;
	SampleOnion_UnpackCoordsAndFace( samples[sampleID], coords, face );
#else
	uint mip;
	Sample_UnpackCoordsAndMip( samples[sampleID], coords, mip );
#endif
	
	// Node levels			: [ 0 .. c_octreeLevelCount-2 ]
	// Leaf level			: c_octreeLevelCount-1

	const uint3 cellCoords = coords >> (c_octreeLevelCount - c_octreeCurrentLevel - 1);
	const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);

	uint childOffset;
	if ( c_octreeCurrentLevel == 0 )
	{		
#if ONION == 1
		childOffset = face * 8 + cellOffset;
#else
		childOffset = mip * 8 + cellOffset;
#endif
	}
	else
	{
		const uint nodeOffset = sampleNodeOffsets[sampleID];
		const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
		childOffset = firstChildOffset + cellOffset;
	}
	
	sampleNodeOffsets[sampleID] = childOffset;

	uint prevFirstChildOffset;
	InterlockedCompareExchange( firstChildOffsets[childOffset], 0, HLSL_NODE_CREATED, prevFirstChildOffset );
		
	[branch]
	if ( prevFirstChildOffset != 0 )
		return;

#if BUILD_INNER == 1
	uint newChildOffset;
	InterlockedAdd( octree_nodeCount, 8, newChildOffset );
	newChildOffset += HLSL_GRID_MAX_COUNT * 8; // root offset

	firstChildOffsets[newChildOffset+0] = 0;
	firstChildOffsets[newChildOffset+1] = 0;
	firstChildOffsets[newChildOffset+2] = 0;
	firstChildOffsets[newChildOffset+3] = 0;
	firstChildOffsets[newChildOffset+4] = 0;
	firstChildOffsets[newChildOffset+5] = 0;
	firstChildOffsets[newChildOffset+6] = 0;
	firstChildOffsets[newChildOffset+7] = 0;

	firstChildOffsets[childOffset] = HLSL_NODE_CREATED | newChildOffset;	
#else	
	uint newLeafID;
	InterlockedAdd( octree_leafCount, 1, newLeafID );
		
	const uint newLeafCount = newLeafID + 1;
	InterlockedMax( octree_leafGroupCountX, HLSL_GROUP_COUNT( newLeafCount, HLSL_OCTREE_THREAD_GROUP_SIZE ) );
	
#if ONION == 1
	octreeLeaves[newLeafID].face3_x9_y9_z11 = (face << 29) | ((coords.x & ~3) << 18) | ((coords.y & ~3) << 9) | (coords.z >> 2);
#else
	octreeLeaves[newLeafID].mip4_none1_x9_y9_z9 = (mip << 28) | ((coords.x & ~3) << 16) | ((coords.y & ~3) << 7) | (coords.z >> 2);
#endif
	octreeLeaves[newLeafID].done1_x2y2z2_count25 = ((coords.x & 3) << 29) | ((coords.y & 3) << 27) | ((coords.z & 3) << 25);
	octreeLeaves[newLeafID].r32 = 0;
	octreeLeaves[newLeafID].g32 = 0;
	octreeLeaves[newLeafID].b32 = 0;

	firstChildOffsets[childOffset] = HLSL_NODE_CREATED | newLeafID;
#endif
}
