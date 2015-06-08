#include "grid_pack.h"

[numthreads( HLSL_GRID_THREAD_GROUP_SIZE, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint blockID = DTid.x;
	const uint blockCount = gridIndirectArgs_blockCount;
	
	if ( blockID < blockCount )
	{
		const GridBlockColor blockColor = gridBlockColors[blockID];
		
		GridBlockPackedColor packedColor;

		[unroll]
		for ( uint cellID = 0; cellID < HLSL_GRID_BLOCK_CELL_COUNT; ++cellID )
		{
			const uint4 gridColor = blockColor.colors[cellID];
			const float invCount = 1.0 / min( 1, gridColor.a );
			const float3 color = min( float3( gridColor.r, gridColor.g, gridColor.b ) * invCount, 255.0 );
						
			packedColor.colors[cellID] = uint( color.r ) << 24 | uint( color.g ) << 16 | uint( color.b ) << 8 | (gridColor.a > 0 ? 255 : 0);
		}

		gridBlockPackedColors[blockID] = packedColor;
	}

	if ( blockID == 0 )
		gridIndirectArgs_instanceCount = blockCount * HLSL_GRID_BLOCK_CELL_COUNT;
}