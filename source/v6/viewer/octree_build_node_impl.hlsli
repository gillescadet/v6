#include "octree_build_node.hlsli"
#include "sample_pack.hlsli"

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint sampleID = DTid.x;
	
	uint3 coords;
	uint mip;
	uint3 color;
	Sample_Unpack( samples[sampleID], coords, mip, color );

	const uint3 cellCoords = coords >> (HLSL_GRID_SHIFT-currentLevel-1);
	const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);

	const uint nodeOffset = currentLevel == 0 ? 0 : sampleNodeOffsets[sampleID];
	const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
	const uint childOffset = firstChildOffset + cellOffset;
	uint prevFirstChildOffset;
	InterlockedCompareExchange( firstChildOffsets[childOffset], 0, HLSL_NODE_CREATED, prevFirstChildOffset );
		
	[branch]
	if ( prevFirstChildOffset != 0 )
		return;
			
#if BUILD_INNER
	uint newChildOffset;
	InterlockedAdd( octree_nodeCount, 8, newChildOffset );

	firstChildOffsets[newChildOffset+0] = 0;
	firstChildOffsets[newChildOffset+1] = 0;
	firstChildOffsets[newChildOffset+2] = 0;
	firstChildOffsets[newChildOffset+3] = 0;
	firstChildOffsets[newChildOffset+4] = 0;
	firstChildOffsets[newChildOffset+5] = 0;
	firstChildOffsets[newChildOffset+6] = 0;
	firstChildOffsets[newChildOffset+7] = 0;

	firstChildOffsets[childOffset] = HLSL_NODE_CREATED | newChildOffset;
	sampleNodeOffsets[sampleID] = childOffset;
#else
	uint newLeafID;
	InterlockedAdd( octree_leaftCount, 1, newLeafID );

	octreeLeaves[newLeafID].r = 0;
	octreeLeaves[newLeafID].g = 0;
	octreeLeaves[newLeafID].b = 0;
	octreeLeaves[newLeafID].x_y = (coords.x << 16) | (coords.y << 16);
	octreeLeaves[newLeafID].z_count = (coords.z << 16);

	firstChildOffsets[childOffset] = HLSL_NODE_CREATED | newLeafID;
	sampleNodeOffsets[sampleID] = newLeafID;
#endif
}
