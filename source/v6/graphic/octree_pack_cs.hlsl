#define HLSL
#include "../graphic/capture_shared.h"

Buffer< uint > firstChildOffsets				: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT );
StructuredBuffer< OctreeLeaf > octreeLeaves		: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT );
Buffer< uint > octreeIndirectArgs 				: REGISTER_SRV( HLSL_OCTREE_INDIRECT_ARGS_SLOT );

RWBuffer< uint > blockPositions					: REGISTER_UAV( HLSL_BLOCK_POS_SLOT );
RWBuffer< uint > blockData						: REGISTER_UAV( HLSL_BLOCK_DATA_SLOT );
RWBuffer< uint > blockIndirectArgs 				: REGISTER_UAV( HLSL_BLOCK_INDIRECT_ARGS_SLOT );

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint leafID = DTid.x;
	if ( leafID >= octree_leafCount )
		return;
		
	const uint buckets[] =			{ 0, 4, 8, 16, 32, 64 };

	const uint blockDataSize[] =	buckets;
	const uint dataSizeInPreviousBucket = blockDataSize[c_octreeCurrentBucket];

	const uint posOffset = block_posOffset( c_octreeCurrentBucket-1 ) + block_count( c_octreeCurrentBucket-1 );
	const uint dataOffset = block_dataOffset( c_octreeCurrentBucket-1 ) + block_count( c_octreeCurrentBucket-1 ) * dataSizeInPreviousBucket;
		
	if ( DTid.x == 0 )
	{
		block_posOffset( c_octreeCurrentBucket ) = posOffset;
		block_dataOffset( c_octreeCurrentBucket ) = dataOffset;
		block_groupCountY( c_octreeCurrentBucket ) = 1;
		block_groupCountZ( c_octreeCurrentBucket ) = 1;
	}

	uint3 coords;
	coords.x = ((octreeLeaves[leafID].x9_r23 >> 21) & ~0x3) | ((octreeLeaves[leafID].x2y2z2_mip4_count15 >> 30) & 0x3);
	coords.y = ((octreeLeaves[leafID].y9_g23 >> 21) & ~0x3) | ((octreeLeaves[leafID].x2y2z2_mip4_count15 >> 28) & 0x3);
	coords.z = ((octreeLeaves[leafID].z9_b23 >> 21) & ~0x3) | ((octreeLeaves[leafID].x2y2z2_mip4_count15 >> 26) & 0x3);

	const uint mip = (octreeLeaves[leafID].x2y2z2_mip4_count15 >> 22) & 0xF;	

	const uint packLevel = c_octreeLevelCount - 3;

	uint nodeOffset;

	for ( uint level = 0; level <= packLevel; ++level )
	{		
		const uint3 cellCoords = coords >> (c_octreeLevelCount - level - 1);
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
		uint slotOccupancyCount = 0;
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
			slotOccupancyCount += countbits( occupancy );
		}	

		InterlockedAdd( block_uniqueOccupancyCount( c_octreeCurrentBucket ), uniqueOccupancyCount );
		InterlockedMax( block_uniqueOccupancyMax( c_octreeCurrentBucket ), uniqueOccupancyCount );
		InterlockedAdd( block_slotOccupancyCount( c_octreeCurrentBucket ), slotOccupancyCount );
	}
#endif // #if HLSL_DEBUG_OCCUPANCY == 1

	InterlockedAdd( block_cellCount( c_octreeCurrentBucket ), cellCount );

	uint blockID;
	InterlockedAdd( block_count( c_octreeCurrentBucket ), 1, blockID );
		
	const uint dataSize = blockDataSize[c_octreeCurrentBucket+1];
	const uint dataBaseID = dataOffset + blockID * dataSize;
	const uint posBaseID = posOffset + blockID;	

	const uint gridMacroShift = c_octreeLevelCount - 2;
	const uint3 blockCoords = coords >> 2;
	const uint blockPos = (blockCoords.z << (gridMacroShift * 2)) | (blockCoords.y << gridMacroShift) | blockCoords.x;
	const uint packedBlockPos = blockPositions[posBaseID] = (mip << 28) | blockPos;
	blockPositions[posBaseID] = packedBlockPos;

	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
		blockData[dataBaseID+cellID] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;

	const uint blockCount = blockID + 1;
	const uint groupCount = HLSL_GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	InterlockedMax( block_groupCountX( c_octreeCurrentBucket ), groupCount );
}
