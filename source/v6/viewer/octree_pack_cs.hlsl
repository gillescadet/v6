#define HLSL
#include "common_shared.h"

Buffer< uint > firstChildOffsets				: register( HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV );
StructuredBuffer< OctreeLeaf > octreeLeaves		: register( HLSL_OCTREE_LEAF_SRV );
Buffer< uint > octreeIndirectArgs 				: register( HLSL_OCTREE_INDIRECT_ARGS_SRV );

RWBuffer< uint > blockColors					: register( HLSL_BLOCK_COLOR_UAV );
RWBuffer< uint > blockIndirectArgs 				: register( HLSL_BLOCK_INDIRECT_ARGS_UAV );

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint leafID = DTid.x;
	if ( leafID >= octree_leafCount )
		return;
		
	const uint buckets[] = { 0, 4, 8, 16, 32, 64 };
	
	const uint packedOffset = block_packedOffset( c_octreeCurrentBucket-1 ) + block_count( c_octreeCurrentBucket-1 ) * (1 + buckets[c_octreeCurrentBucket]);
		
	if ( DTid.x == 0 )
	{
		block_packedOffset( c_octreeCurrentBucket ) = packedOffset;
		block_vertexCountPerInstance( c_octreeCurrentBucket ) = 1;
		block_indexCountPerInstance( c_octreeCurrentBucket ) = 36;
		block_cellGroupCountY( c_octreeCurrentBucket ) = 1;
		block_cellGroupCountZ( c_octreeCurrentBucket ) = 1;
	}

	uint3 coords;
	coords.x = ((octreeLeaves[leafID].x8_r24 >> 20) & ~0xF) | ((octreeLeaves[leafID].x4y4z4_mip4_count16 >> 28) & 0xF);
	coords.y = ((octreeLeaves[leafID].y8_g24 >> 20) & ~0xF) | ((octreeLeaves[leafID].x4y4z4_mip4_count16 >> 24) & 0xF);
	coords.z = ((octreeLeaves[leafID].z8_b24 >> 20) & ~0xF) | ((octreeLeaves[leafID].x4y4z4_mip4_count16 >> 20) & 0xF);

	const uint mip = (octreeLeaves[leafID].x4y4z4_mip4_count16 >> 16) & 0xF;

	const uint packLevel = HLSL_GRID_SHIFT-3;

	uint nodeOffset;

	for ( uint level = 0; level <= packLevel; ++level )
	{		
		const uint3 cellCoords = coords >> (HLSL_GRID_SHIFT-level-1);
		const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);

		int childOffset;
		if ( level == 0 )
		{
			childOffset = mip * 8 + cellOffset;
		}
		else
		{
			const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
			childOffset = firstChildOffset + cellOffset;			
		}				
		nodeOffset = childOffset;		
	}

	uint cellRGBA[64];
	uint cellCount = 0;
	uint firstLeafID = 0xFFFFFFFF;

	const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;

	const uint cellMinCount = buckets[c_octreeCurrentBucket]+1;
	const uint cellMaxCount = buckets[c_octreeCurrentBucket+1];

	for ( uint childID0 = 0; childID0 < 8; ++childID0 )
	{
		uint childNodeOffset0 = firstChildOffsets[firstChildOffset+childID0];
		if ( childNodeOffset0 )
		{
			childNodeOffset0 &= ~HLSL_NODE_CREATED;

			for ( uint childID1 = 0; childID1 < 8; ++childID1 )
			{
				uint childLeafID = firstChildOffsets[childNodeOffset0+childID1];
				if ( childLeafID )
				{
					childLeafID &= ~HLSL_NODE_CREATED;

					firstLeafID = firstLeafID == 0xFFFFFFFF ? childLeafID  : firstLeafID;

					[branch]
					if ( firstLeafID != leafID )
						return;

					const uint sampleCount = octreeLeaves[childLeafID].x4y4z4_mip4_count16 & 0xFFFF;
					const uint r = (octreeLeaves[childLeafID].x8_r24 & 0x00FFFFFF) / sampleCount;
					const uint g = (octreeLeaves[childLeafID].y8_g24 & 0x00FFFFFF) / sampleCount;
					const uint b = (octreeLeaves[childLeafID].z8_b24 & 0x00FFFFFF) / sampleCount;
					const uint cellPos = ((childID0&4)<<3) | ((childID1&4)<<2) | ((childID0&2)<<2) | ((childID1&2)<<1) | ((childID0&1)<<1) | ((childID1&1)<<0);
					cellRGBA[cellCount] = (r << 24) | (g << 16) | (b << 8) | ((mip & 3) << 6) | cellPos;
					++cellCount;

					[branch]
					if ( cellCount > cellMaxCount )
						return;
				}
			}
		}
	}	
	
	[branch]
	if ( cellCount < cellMinCount )
		return;

	InterlockedAdd( block_cellCount( c_octreeCurrentBucket ), cellCount );
			
	uint blockID;
	InterlockedAdd( block_count( c_octreeCurrentBucket ), 1, blockID );
		
	const uint packedCount = 1 + cellMaxCount;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint3 blockCoords = coords >> HLSL_GRID_BLOCK_SHIFT;
	const uint blockPos = (blockCoords.z << HLSL_GRID_MACRO_2XSHIFT) | (blockCoords.y << HLSL_GRID_MACRO_SHIFT) | blockCoords.x;
	blockColors[packedBaseID] = ((mip & 0xC) << 28) | blockPos;

	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
		blockColors[packedBaseID + cellID + 1] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;

	const uint blockCount = blockID + 1;
	const uint instanceCount = blockCount * cellMaxCount;
	InterlockedMax( block_renderInstanceCount( c_octreeCurrentBucket ), instanceCount );
	InterlockedMax( block_instanceCount( c_octreeCurrentBucket ), instanceCount );

	const uint groupCount = GROUP_COUNT( instanceCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	InterlockedMax( block_cellGroupCountX( c_octreeCurrentBucket ), groupCount );
}
