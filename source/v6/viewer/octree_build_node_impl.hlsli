#define HLSL

#include "common_shared.h"
#include "sample_pack.hlsli"

StructuredBuffer< Sample > samples				: register( HLSL_SORTED_SAMPLE_SRV );
Buffer< uint > sortedSampleIndirectArgs 		: register( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_SRV );

RWBuffer< uint > sampleNodeOffsets 				: register( HLSL_OCTREE_SAMPLE_NODE_OFFSET_UAV );
RWBuffer< uint > firstChildOffsets				: register( HLSL_OCTREE_FIRST_CHILD_OFFSET_UAV );
RWStructuredBuffer< OctreeLeaf > octreeLeaves	: register( HLSL_OCTREE_LEAF_UAV );
RWBuffer< uint > octreeIndirectArgs 			: register( HLSL_OCTREE_INDIRECT_ARGS_UAV );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint sampleID = DTid.x;
		
	if ( sampleID >= sortedSample_count( c_octreeCurrentMip ) )
		return;

#if BUILD_INNER == 0
	const uint newLeafOffset = octree_leafSum( c_octreeCurrentMip-1 ) + octree_leafCount( c_octreeCurrentMip-1 );

	if ( DTid.x == 0 )
	{
		octree_leafSum( c_octreeCurrentMip ) = newLeafOffset;
		octree_leafGroupCountY( c_octreeCurrentMip ) = 1;
		octree_leafGroupCountZ( c_octreeCurrentMip ) = 1;
	}
#endif // #if BUILD_INNER == 0

	sampleID += sortedSample_sum( c_octreeCurrentMip );

	const uint3 coords = Sample_UnpackCoords( samples[sampleID] );
	
	// Node levels			: [ 0 .. HLSL_GRID_SHIFT-2 ]
	// Leaf level			: HLSL_GRID_SHIFT-1

	const uint3 cellCoords = coords >> (HLSL_GRID_SHIFT-c_octreeCurrentLevel-1);
	const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);	

	uint childOffset;
	if ( c_octreeCurrentLevel == 0 )
	{		
		childOffset = c_octreeCurrentMip * 8 + cellOffset;
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
	InterlockedAdd( octree_leafCount( c_octreeCurrentMip ), 1, newLeafID );
		
	const uint newLeafCount = newLeafID + 1;
	InterlockedMax( octree_leafGroupCountX( c_octreeCurrentMip ), GROUP_COUNT( newLeafCount, HLSL_OCTREE_THREAD_GROUP_SIZE ) );
	
	newLeafID += newLeafOffset;

	octreeLeaves[newLeafID].x8_r = (coords.x & ~0xF) << 20;
	octreeLeaves[newLeafID].y8_g = (coords.y & ~0xF) << 20;
	octreeLeaves[newLeafID].z8_b = (coords.z & ~0xF) << 20;
	octreeLeaves[newLeafID].x4y4z4_count = (coords.x & 0xF) << 28 | (coords.y & 0xF) << 24 | (coords.z & 0xF) << 20;

	firstChildOffsets[childOffset] = HLSL_NODE_CREATED | newLeafID;
#endif
}
