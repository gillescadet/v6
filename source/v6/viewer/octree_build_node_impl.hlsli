#define HLSL

#include "common_shared.h"
#include "sample_pack.hlsli"

StructuredBuffer< Sample > samples				: register( HLSL_SAMPLE_SRV );
Buffer< uint > sampleIndirectArgs 				: register( HLSL_SAMPLE_INDIRECT_ARGS_SRV );

RWBuffer< uint > sampleNodeOffsets 				: register( HLSL_OCTREE_SAMPLE_NODE_OFFSET_UAV );
RWBuffer< uint > firstChildOffsets				: register( HLSL_OCTREE_FIRST_CHILD_OFFSET_UAV );
RWStructuredBuffer< OctreeLeaf > octreeLeaves	: register( HLSL_OCTREE_LEAF_UAV );
RWBuffer< uint > octreeIndirectArgs 			: register( HLSL_OCTREE_INDIRECT_ARGS_UAV );

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
	uint mip;
	Sample_UnpackCoordsAndMip( samples[sampleID], coords, mip );
	
	// Node levels			: [ 0 .. HLSL_GRID_SHIFT-2 ]
	// Leaf level			: HLSL_GRID_SHIFT-1

	const uint3 cellCoords = coords >> (HLSL_GRID_SHIFT-c_octreeCurrentLevel-1);
	const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);	

	uint childOffset;
	if ( c_octreeCurrentLevel == 0 )
	{		
		childOffset = mip * 8 + cellOffset;
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
	newChildOffset += HLSL_MIP_MAX_COUNT * 8;

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
	InterlockedMax( octree_leafGroupCountX, GROUP_COUNT( newLeafCount, HLSL_OCTREE_THREAD_GROUP_SIZE ) );
	
	octreeLeaves[newLeafID].x9_r23 = (coords.x & ~0x3) << 21;
	octreeLeaves[newLeafID].y9_g23 = (coords.y & ~0x3) << 21;
	octreeLeaves[newLeafID].z9_b23 = (coords.z & ~0x3) << 21;
	octreeLeaves[newLeafID].x2y2z2_mip4_count15 = (coords.x & 0x3) << 30 | (coords.y & 0x3) << 28 | (coords.z & 0x3) << 26 | (mip << 22);
	octreeLeaves[newLeafID].occupancy27 = 0;

	firstChildOffsets[childOffset] = HLSL_NODE_CREATED | newLeafID;
#endif
}
