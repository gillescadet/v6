#include "grid_pack.h"

void packCells( inout RWBuffer< uint > packedColors, uint blockPos, const GridBlockColor blockColor, uint bucket )
{
	uint blockID;
	InterlockedAdd( gridIndirectArgs_packedBlockCounts( bucket ), 1, blockID );

	const uint cellCount = 1 << (bucket + 2);
	const uint packedCount = 1 + cellCount;
	
	uint packedID = blockID * packedCount;	
	uint packedRank = 0;
	packedColors[packedID + packedRank] = blockPos;
	++packedRank;

	[unroll]	
	for ( uint cellPos = 0; cellPos < HLSL_GRID_BLOCK_CELL_COUNT; ++cellPos )
	{
		const uint4 gridColor = blockColor.colors[cellPos];		
		if ( gridColor.a > 0 )				
		{
			const float invCount = 1.0 / gridColor.a;
			const float3 color = min( float3( gridColor.r, gridColor.g, gridColor.b ) * invCount, 255.0 );
			packedColors[packedID + packedRank] = (uint( color.r ) << 24) | (uint( color.g ) << 16) | (uint( color.b ) << 8) | cellPos;
			++packedRank;
			if ( packedRank == packedCount )
				break;
		}
	}
		
	while ( packedRank < packedCount )
	{
		packedColors[packedID + packedRank] = HLSL_GRID_BLOCK_CELL_EMPTY;
		++packedRank;
	}

	const uint blockCount = blockID + 1;
	InterlockedMax( gridIndirectArgs_renderInstanceCount( bucket ), blockCount * cellCount );
}

[numthreads( HLSL_GRID_THREAD_GROUP_SIZE, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint blockID = DTid.x;
	const uint blockCount = gridIndirectArgs_blockCount;
	
	if ( blockID < blockCount )
	{	
		const GridBlockColor blockColor = gridBlockColors[blockID];
		
		uint filledCellCount = 0;
		[unroll]
		for ( uint cellPos = 0; cellPos < HLSL_GRID_BLOCK_CELL_COUNT; ++cellPos )
			filledCellCount += blockColor.colors[cellPos].a > 0 ? 1 : 0;

		const uint blockPos = gridBlockPositions[blockID];
		
		if ( filledCellCount <= 4 )
			packCells( gridBlockPackedColors4, blockPos, blockColor, 0 );
		else if ( filledCellCount <= 8 )
			packCells( gridBlockPackedColors8, blockPos, blockColor, 1 );
		else if ( filledCellCount <= 16 )
			packCells( gridBlockPackedColors16, blockPos, blockColor, 2 );
		else if ( filledCellCount <= 32 )
			packCells( gridBlockPackedColors32, blockPos, blockColor, 3 );
		else
			packCells( gridBlockPackedColors64, blockPos, blockColor, 4 );

#if HLSL_GRIDBLOCK_CELL_STATS
		InterlockedAdd( gridIndirectArgs_cellCount, filledCellCount );
#endif
	}
}