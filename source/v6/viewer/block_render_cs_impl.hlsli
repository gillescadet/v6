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

	if ( !PackedColor_Unpack( packedID, blockCell ) )
		return;

	const matrix mx = mul( c_blockViewToProj, c_blockObjectToView );

#if HLSL_TRACE_USE_ALIGNED_QUAD == 1
	const float3 vertices[8] =
	{
		float3( -1.0f, -1.0f, -1.0f ),
		float3( -1.0f, -1.0f,  1.0f ),
		float3( -1.0f,  1.0f, -1.0f ),
		float3( -1.0f,  1.0f,  1.0f ),
		float3(  1.0f, -1.0f, -1.0f ),
		float3(  1.0f, -1.0f,  1.0f ),
		float3(  1.0f,  1.0f, -1.0f ),
		float3(  1.0f,  1.0f,  1.0f ),
	};

	float2 minScreenPos = float2(  1e32f,  1e32f );
	float2 maxScreenPos = float2( -1e32f, -1e32f );
	float pixelDepth = 1e32f;
		
	uint clippedCount = 0;
	for ( uint vertexID = 0; vertexID < 8; ++vertexID )
	{
		const float3 posOS = blockCell.posOS + vertices[vertexID] * blockCell.halfCellSize;
		const float4 posCS = mul( mx, float4( posOS, 1.0f ) );
		const float2 screenPos = posCS.xy * rcp( posCS.w );		
		minScreenPos = min( minScreenPos, screenPos );
		maxScreenPos = max( maxScreenPos, screenPos );
		pixelDepth = min( pixelDepth, posCS.w );
		clippedCount += any( abs( posCS.xyz ) > posCS.w ) ? 1 : 0;
	}

	if ( clippedCount == 8 )
		return;

	const float2 screenPos = ( minScreenPos + maxScreenPos ) * 0.5f;
	const float2 screenOffset = ( maxScreenPos - minScreenPos ) * 0.5f;
	const float2 pixelOrg = mad( screenPos, 0.5f, 0.5f ) * c_blockFrameSize;
	const float2 pixelOffset = screenOffset * 0.5f * c_blockFrameSize;
	const uint2 pixelUV = clamp( int2( frac( pixelOrg ) * 255.0f + 0.5f ), 0, 255 );
	const uint2 pixelSize = clamp( int2( saturate( pixelOffset ) * 255.0f + 0.5f ), 0, 255 );
#else // #if HLSL_TRACE_USE_ALIGNED_QUAD == 1
	const float4 posCS = mul( mx, float4( blockCell.posOS, 1.0 ) );

	if ( any( abs( posCS.xyz ) > posCS.w ) )
		return;

	const float2 pixelOrg = mad( posCS.xy / posCS.w, 0.5f, 0.5f ) * c_blockFrameSize;
	const uint2 pixelUV = clamp( frac( pixelOrg ) * BUFFER_WIDTH, 0, BUFFER_WIDTH-1 );
	const uint2 pixelSize = uint2( BUFFER_WIDTH/2, BUFFER_WIDTH/2 );
	const float pixelDepth = posCS.w;
#endif // #if HLSL_TRACE_USE_ALIGNED_QUAD != 1

	uint blockCellItemID;
	InterlockedAdd( blockContext[0].cellItemCount, 1, blockCellItemID );
	blockCellItemID += 1;

	const int2 pixelCoords = int2( pixelOrg );
	const uint pixelID = pixelCoords.y * c_blockFrameSize.x + pixelCoords.x;

	uint nextBlockCellItemID;
	InterlockedExchange( firstBlockCellItemIDs[pixelID], blockCellItemID, nextBlockCellItemID );
	
	BlockCellItem blockCellItem;
	blockCellItem.r8g8b8a8 = (blockCell.color & ~0xFF) | 0xFF;
	blockCellItem.u8v8w8h8 = (pixelUV.x << 24) | (pixelUV.y << 16) | (pixelSize.x << 8) | (pixelSize.y << 0);
	blockCellItem.depth = pixelDepth;
	blockCellItem.nextID = nextBlockCellItemID;

	blockCellItems[blockCellItemID] = blockCellItem;	
}
