#define HLSL

#include "common_shared.h"
#include "block_cell.hlsli"

#define BUFFER_WIDTH HLSL_PIXEL_SUPER_SAMPLING_WIDTH

RWStructuredBuffer< BlockCellItem > blockCellItems			: register( HLSL_BLOCK_CELL_ITEM_UAV );
RWBuffer< uint > firstBlockCellItemIDs						: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV );
RWStructuredBuffer< BlockContext > blockContext				: register( HLSL_BLOCK_CONTEXT_UAV );

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint packedID = DTid.x;
	
	const uint maxCellCount = blockIndirectArgs[block_count_offset( GRID_CELL_BUCKET )] * GRID_CELL_COUNT;

	if ( packedID >= maxCellCount )
		return;

	BlockCell blockCell;

	if ( !PackedColor_Unpack( packedID, blockCell, -1 ) )
		return;

	uint blockCellItemID;
	InterlockedAdd( blockContext[0].cellItemCount, 1, blockCellItemID );
	blockCellItemID += 1;

	const float2 pixelOrg = mad( blockCell.posCS.xy / blockCell.posCS.w, 0.5f, 0.5f ) * c_blockFrameSize;	
	const int2 pixelCoords = int2( pixelOrg );

#if 0
	if ( pixelCoords.x < 0 || pixelCoords.x >= c_blockFrameSize.x || pixelCoords.y < 0 || pixelCoords.y >= c_blockFrameSize.y )
		return;
#endif

	const uint pixelID = pixelCoords.y * c_blockFrameSize.x + pixelCoords.x;

	uint nextBlockCellItemID;
	InterlockedExchange( firstBlockCellItemIDs[pixelID], blockCellItemID, nextBlockCellItemID );
	
	const uint2 pixelUV = clamp( frac( pixelOrg ) * BUFFER_WIDTH, 0, BUFFER_WIDTH-1 );

	BlockCellItem blockCellItem;
	blockCellItem.r8g8b8_u4v4 = (blockCell.color & ~0xFF) | (pixelUV.x << 4) | pixelUV.y;
	blockCellItem.depth = blockCell.posCS.w;
	blockCellItem.nextID = nextBlockCellItemID;

	blockCellItems[blockCellItemID] = blockCellItem;	
}
