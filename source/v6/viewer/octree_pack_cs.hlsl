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
	
	const uint packedCountInPreviousBucket = 1 + buckets[c_octreeCurrentBucket] * HLSL_COUNT;
	const uint packedOffset = block_packedOffset( c_octreeCurrentBucket-1 ) + block_count( c_octreeCurrentBucket-1 ) * packedCountInPreviousBucket;
		
	if ( DTid.x == 0 )
	{
		block_packedOffset( c_octreeCurrentBucket ) = packedOffset;
		block_vertexCountPerInstance( c_octreeCurrentBucket ) = 1;
		block_indexCountPerInstance( c_octreeCurrentBucket ) = 36;
		block_indexCountPerInstance2( c_octreeCurrentBucket ) = 36;
		block_groupCountY( c_octreeCurrentBucket ) = 1;
		block_groupCountZ( c_octreeCurrentBucket ) = 1;
		block_cellGroupCountY( c_octreeCurrentBucket ) = 1;
		block_cellGroupCountZ( c_octreeCurrentBucket ) = 1;
	}

	uint3 coords;
	coords.x = ((octreeLeaves[leafID].x9_r23 >> 21) & ~0x3) | ((octreeLeaves[leafID].x2y2z2_mip4_count15 >> 30) & 0x3);
	coords.y = ((octreeLeaves[leafID].y9_g23 >> 21) & ~0x3) | ((octreeLeaves[leafID].x2y2z2_mip4_count15 >> 28) & 0x3);
	coords.z = ((octreeLeaves[leafID].z9_b23 >> 21) & ~0x3) | ((octreeLeaves[leafID].x2y2z2_mip4_count15 >> 26) & 0x3);

	const uint mip = (octreeLeaves[leafID].x2y2z2_mip4_count15 >> 22) & 0xF;	

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
	uint cellOccupancy[64];
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

					const uint sampleCount = octreeLeaves[childLeafID].x2y2z2_mip4_count15 & 0x7FFF;
					const uint r = (octreeLeaves[childLeafID].x9_r23 & 0x007FFFFF) / sampleCount;
					const uint g = (octreeLeaves[childLeafID].y9_g23 & 0x007FFFFF) / sampleCount;
					const uint b = (octreeLeaves[childLeafID].z9_b23 & 0x007FFFFF) / sampleCount;
					const uint cellPos = ((childID0&4)<<3) | ((childID1&4)<<2) | ((childID0&2)<<2) | ((childID1&2)<<1) | ((childID0&1)<<1) | ((childID1&1)<<0);
					cellRGBA[cellCount] = (r << 24) | (g << 16) | (b << 8) | cellPos;

					cellOccupancy[cellCount] = octreeLeaves[childLeafID].occupancy27;
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

#if HLSL_DEBUG_OCCUPANCY == 1
	{
		uint uniqueOccupancyCount = 0;
		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint occupancy = cellOccupancy[cellID];
			uint cellOccupancyID = 0;
			for ( cellOccupancyID = 0; cellOccupancyID < cellID; ++cellOccupancyID )
			{
				if ( cellOccupancy[cellOccupancyID] == occupancy )
				{
					break;
				}
			}
			uniqueOccupancyCount += cellOccupancyID == cellID ? 1 : 0;
		}	

		InterlockedAdd( block_uniqueOccupancyCount( c_octreeCurrentBucket ), uniqueOccupancyCount );
		InterlockedMax( block_uniqueOccupancyMax( c_octreeCurrentBucket ), uniqueOccupancyCount );
	}
#endif // #if HLSL_DEBUG_OCCUPANCY == 1

	InterlockedAdd( block_cellCount( c_octreeCurrentBucket ), cellCount );
			
	uint blockID;
	InterlockedAdd( block_count( c_octreeCurrentBucket ), 1, blockID );
		
	const uint packedCount = 1 + cellMaxCount * HLSL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint3 blockCoords = coords >> HLSL_GRID_BLOCK_SHIFT;
	const uint blockPos = (blockCoords.z << HLSL_GRID_MACRO_2XSHIFT) | (blockCoords.y << HLSL_GRID_MACRO_SHIFT) | blockCoords.x;
	blockColors[packedBaseID] = (mip << 28) | blockPos;

	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
	{
		const uint packedRank = packedBaseID + 1 + cellID * HLSL_COUNT;
		blockColors[packedRank + 0] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;
#if HLSL_COUNT == 2
		blockColors[packedRank + 1] = cellID < cellCount ? cellOccupancy[cellID] : 0;
#endif // #if HLSL_COUNT == 2
	}

	const uint blockCount = blockID + 1;
	const uint groupCount = GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	InterlockedMax( block_groupCountX( c_octreeCurrentBucket ), groupCount );

	const uint instanceCount = blockCount * cellMaxCount;
	InterlockedMax( block_renderInstanceCount( c_octreeCurrentBucket ), instanceCount );
	InterlockedMax( block_instanceCount( c_octreeCurrentBucket ), instanceCount );
	InterlockedMax( block_instanceCount2( c_octreeCurrentBucket ), instanceCount * HLSL_PIXEL_SUPER_SAMPLING_WIDTH_CUBE );

	const uint groupCellCount = GROUP_COUNT( instanceCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	InterlockedMax( block_cellGroupCountX( c_octreeCurrentBucket ), groupCellCount );
}
