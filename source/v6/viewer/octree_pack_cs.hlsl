#include "octree_pack.hlsli"

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint leafID = DTid.x;
	uint3 coords;
	coords.x = octreeLeaves[leafID].x_y >> 16;
	coords.y = octreeLeaves[leafID].x_y & 0xFFFF;
	coords.z = octreeLeaves[leafID].z_count >> 16;
	
	uint nodeOffset = 0;

	uint packLevel = HLSL_GRID_SHIFT-2;

	[unroll]
	for ( uint level = 0; level <= packLevel; ++level )
	{
		const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
		const uint3 cellCoords = coords >> (HLSL_GRID_SHIFT-level-1);
		const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);
		const uint childOffset = firstChildOffset + cellOffset;	
		nodeOffset = firstChildOffsets[childOffset] & ~HLSL_NODE_CREATED;
	}

	uint cellRGBA[64];
	uint cellCount = 0;
	uint firstLeafID = 0xFFFFFFFF;

	[unroll]
	for ( uint childID0 = 0; childID0 < 8; ++childID0 )
	{
		const uint childNodeOffset0 = firstChildOffsets[nodeOffset+childID0] & ~HLSL_NODE_CREATED;
		if ( childNodeOffset0 )
		{
			[unroll]
			for ( uint childID1 = 0; childID1 < 8; ++childID1 )
			{
				const uint child1LeafID = firstChildOffsets[childNodeOffset0+childID1] & ~HLSL_NODE_CREATED;
				if ( child1LeafID )
				{
					firstLeafID = min( firstLeafID, child1LeafID );

					const uint count = octreeLeaves[leafID].z_count & 0xFFFF;
					const uint r = min( 0, octreeLeaves[leafID].r / count );
					const uint g = min( 0, octreeLeaves[leafID].g / count );
					const uint b = min( 0, octreeLeaves[leafID].b / count );
					const uint cellPos = ((childID0&4)<<3) | ((childID1&4)<<2) | ((childID0&2)<<2) | ((childID1&2)<<1) | ((childID0&1)<<1) | ((childID1&1)<<0);
					cellRGBA[count] = (r << 24) | (b << 16) | (b << 8) | cellPos;
					++cellCount;
				}
			}
		}
	}	

	const uint buckets[] = { 0, 4, 8, 16, 32, 64 };
	const uint cellMinCount = buckets[currentBucket]+1;
	const uint cellMaxCount = buckets[currentBucket+1];

	[branch]
	if ( firstLeafID != leafID || cellCount < cellMinCount || cellCount > cellMaxCount )
		return;

	const uint blockPos = (coords.z << HLSL_GRID_MACRO_2XSHIFT) | (coords.y << HLSL_GRID_MACRO_SHIFT) | coords.x;
			
	const uint blockOffset = octree_blockSum( currentBucket-2 ) + octree_blockCount( currentBucket-1 );
	uint blockID;
	InterlockedAdd( octree_blockCount( currentBucket ), 1, blockID );	
	blockID += blockOffset;
		
	const uint packedCount = 1 + cellCount;
	
	const uint packedBaseID = (blockOffset + blockID) * packedCount;	
	packedColors[packedBaseID] = blockPos;	

	[unroll]
	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
		packedColors[packedBaseID + cellID + 1] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;

	const uint blockCount = blockID + 1;
	InterlockedMax( grid_renderInstanceCount( currentBucket ), blockCount * cellMaxCount );

	if ( DTid.x == 0 )
		octree_blockSum( currentBucket-1 ) = blockOffset;
}
