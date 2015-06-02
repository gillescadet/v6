#include "grid_pack.h"

[numthreads( HLSL_GRID_PACK_GROUP_SIZE, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint gridBlockPos = DTid.x;
	
	[branch]
	if ( gridBlockAssignedIDs[gridBlockPos] != HLSL_GRID_BLOCK_INVALID )
	{
		GridBlockPackedColor packedColor;
		packedColor.blockPos = gridBlockPos;

		const uint blockID = gridBlockAssignedIDs[gridBlockPos];
		const GridBlockColor blockColor = gridBlockColors[blockID];
		
		[unroll]
		for ( uint cellID = 0; cellID < HLSL_GRID_BLOCK_CELL_COUNT; ++cellID )
		{
			const GridColor gridColor = blockColor.colors[cellID];
			const float invCount = 1.0 / gridColor.a;
			const float3 color = float3( gridColor.r, gridColor.g, gridColor.b ) * invCount;
						
			const uint rgb = uint( color.r ) << 24 | uint( color.g ) << 16 | uint( color.g ) << 8;
			packedColor.colors[cellID].rgba = rgb | (rgb != 0 ? 255 : 0);			
		}

		uint packedBlockID;
		InterlockedAdd( gridIndirectArgs[0].blockCount, 1, packedBlockID);
		gridBlockPackedColors[packedBlockID] = packedColor;

		const uint blockCount = packedBlockID+1;
		InterlockedMax( gridIndirectArgs[0].instanceCount, blockCount * HLSL_GRID_BLOCK_CELL_COUNT );

		
		
	}
}