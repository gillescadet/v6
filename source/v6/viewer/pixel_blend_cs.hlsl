#define HLSL
#include "common_shared.h"

#define BUFFER_WIDTH HLSL_PIXEL_SUPER_SAMPLING_WIDTH

StructuredBuffer< BlockCellItem > blockCellItems				: register( HLSL_BLOCK_CELL_ITEM_SRV );
Buffer< uint > firstBlockCellItemIDs							: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_SRV );

RWTexture2D< float4 > outputColors								: register( HLSL_COLOR_UAV );
#if HLSL_DEBUG_PIXEL == 1
RWStructuredBuffer< PixelBlendDebugBuffer > pixelDebugBuffer	: register( HLSL_PIXEL_DEBUG_UAV );
#endif // #if HLSL_DEBUG_PIXEL == 1

static const float s_depthMax = 1e32f;

#if HLSL_DEBUG_PIXEL == 1
static const uint s_pixelMode = c_pixelMode;
#else
static const uint s_pixelMode = 0;
#endif

float3 testAlignedQuadA( uint2 screenPos, bool debug )
{
	float depthBuffer[BUFFER_WIDTH][BUFFER_WIDTH];
	float3 colorBuffer[BUFFER_WIDTH][BUFFER_WIDTH];

	{
		for ( uint v = 0; v < BUFFER_WIDTH; ++v )
		{
			for ( uint u = 0; u < BUFFER_WIDTH; ++u )
			{
				depthBuffer[v][u] = s_depthMax;
				colorBuffer[v][u] = float3( 0.0f, 0.0f, 0.0f );
			}
		}
	}
	
	const uint pixelID = screenPos.y * c_pixelFrameSize.x + screenPos.x;
			
	BlockCellItem blockCellItem;
	for ( uint blockCellItemID = firstBlockCellItemIDs[pixelID]; blockCellItemID != 0; blockCellItemID = blockCellItem.nextID )
	{
		blockCellItem = blockCellItems[blockCellItemID];
			
		const float inv255 = 1.0f / 255.0f;
		const float r = ((blockCellItem.r8g8b8a8 >> 24) & 0xFF) * inv255;
		const float g = ((blockCellItem.r8g8b8a8 >> 16) & 0xFF) * inv255;
		const float b = ((blockCellItem.r8g8b8a8 >> 8) & 0xFF) * inv255;
		const float3 otherColor = float3( r, g, b );
				
		const uint occupancy = blockCellItem.u8v8w8h8;

#if HLSL_DEBUG_PIXEL == 1
		const uint layer = pixelDebugBuffer[0].layerCount;
		if ( debug && layer < 4 )
		{				
			PixelBlendDebugLayer debugLayer;
			debugLayer.colorAndDepth = float4( otherColor, blockCellItem.depth );
			debugLayer.occupancy = occupancy;

			pixelDebugBuffer[0].layers[layer] = debugLayer;
			++pixelDebugBuffer[0].layerCount;
		}
#endif // #if HLSL_DEBUG_PIXEL == 1				

		uint occupancyBit = 0;
		for ( uint v = 0; v < BUFFER_WIDTH; ++v )
		{
			for ( uint u = 0; u < BUFFER_WIDTH; ++u, ++occupancyBit )
			{
				if ( (occupancy & (1 << occupancyBit)) != 0 && blockCellItem.depth < depthBuffer[v][u] )
				{
					depthBuffer[v][u] = blockCellItem.depth;
					colorBuffer[v][u] = otherColor;
				}
			}
		}				
	}
	
	float3 finalColor = float3( 0.0f, 0.0f, 0.0f );
	float rasterCount = 0;

	{
		for ( uint v = 0; v < BUFFER_WIDTH; ++v )
		{
			for ( uint u = 0; u < BUFFER_WIDTH; ++u )
			{
				finalColor += colorBuffer[v][u];
				rasterCount += depthBuffer[v][u] < s_depthMax ? 1.0f : 0.0f;

#if HLSL_DEBUG_PIXEL == 1
				if ( debug )
				{
					pixelDebugBuffer[0].colorBuffer[v][u] = colorBuffer[v][u];
					pixelDebugBuffer[0].depthBuffer[v][u] = depthBuffer[v][u];
				}
#endif // #if HLSL_DEBUG_PIXEL == 1
			}
		}
	}
	
	if ( rasterCount == 0 )
		return c_pixelBackColor;
	else
		return finalColor / rasterCount;
	// return finalColor / 16.0f;
}

[ numthreads( 16, 16, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
#if HLSL_DEBUG_PIXEL == 1
	const bool debug = c_pixelDebug != 0 && c_pixelDebugCoords.x == DTid.x && c_pixelDebugCoords.y == DTid.y;
#else
	const bool debug = false;
#endif // #if HLSL_DEBUG_PIXEL == 1
	
	const uint2 screenPos = uint2( DTid.x, c_pixelFrameSize.y - DTid.y - 1 );

	if ( s_pixelMode == 0)
		outputColors[DTid.xy] = float4( testAlignedQuadA( screenPos, debug ), 1.0f );
}
