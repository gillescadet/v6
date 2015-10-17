#define HLSL
#include "common_shared.h"

#define BUFFER_WIDTH HLSL_PIXEL_SUPER_SAMPLING_WIDTH

StructuredBuffer< BlockCellItem > blockCellItems		: register( HLSL_BLOCK_CELL_ITEM_SRV );
Buffer< uint > firstBlockCellItemIDs					: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_SRV );

RWTexture2D< float4 > outputColors						: register( HLSL_COLOR_UAV );
#if HLSL_DEBUG_PIXEL == 1
RWStructuredBuffer< PixelDebugBuffer > pixelDebugBuffer	: register( HLSL_PIXEL_DEBUG_UAV );
#endif // #if HLSL_DEBUG_PIXEL == 1

static const float s_depthMax = 1e32f;

float3 testA( uint2 screenPos, bool debug )
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

	for ( int j = -1; j <= 1; ++j )
	{
		for ( int i = -1; i <= 1; ++i )
		{
			const uint otherPxelID = (screenPos.y + j) * c_pixelFrameSize.x + screenPos.x + i;
			
			BlockCellItem blockCellItem;
			for ( uint blockCellItemID = firstBlockCellItemIDs[otherPxelID]; blockCellItemID != 0; blockCellItemID = blockCellItem.nextID )
			{
				blockCellItem = blockCellItems[blockCellItemID];
			
				const float inv255 = 1.0f / 255.0f;
				const float r = ((blockCellItem.r8g8b8_u4v4 >> 24) & 0xFF) * inv255;
				const float g = ((blockCellItem.r8g8b8_u4v4 >> 16) & 0xFF) * inv255;
				const float b = ((blockCellItem.r8g8b8_u4v4 >> 8) & 0xFF) * inv255;
				const float3 otherColor = float3( r, g, b );
				
				const int uOffset = ((blockCellItem.r8g8b8_u4v4 >> 4) & 0xF) - BUFFER_WIDTH/2;
				const int vOffset = ((blockCellItem.r8g8b8_u4v4 >> 0) & 0xF) - BUFFER_WIDTH/2;

#if HLSL_DEBUG_PIXEL == 1
				const uint layer = pixelDebugBuffer[0].points[j+1][i+1].layerCount;
				if ( debug && layer < 4 )
				{				
					PixelDebugLayer debugLayer;
					debugLayer.color = float4( otherColor, 0.0f );
					debugLayer.depth = blockCellItem.depth;
					debugLayer.uv = float2( uOffset, vOffset );

					pixelDebugBuffer[0].points[j+1][i+1].layers[layer] = debugLayer;
					++pixelDebugBuffer[0].points[j+1][i+1].layerCount;
				}
#endif // #if HLSL_DEBUG_PIXEL == 1

				const int2 posMin = int2( i * BUFFER_WIDTH + uOffset, j * BUFFER_WIDTH + vOffset );
				const int uMin = max( posMin.x, 0 );
				const int vMin = max( posMin.y, 0 );
				const int uMax = min( posMin.x+BUFFER_WIDTH, BUFFER_WIDTH-1 );
				const int vMax = min( posMin.y+BUFFER_WIDTH, BUFFER_WIDTH-1 );

				for ( int v = vMin; v <= vMax; ++v )
				{
					for ( int u = uMin; u <= uMax; ++u )
					{
						if ( blockCellItem.depth < depthBuffer[v][u] )
						{
							depthBuffer[v][u] = blockCellItem.depth;
							colorBuffer[v][u] = otherColor;
						}
					}
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
					pixelDebugBuffer[0].depthBuffer[v][u] = depthBuffer[v][u];
					pixelDebugBuffer[0].colorBuffer[v][u] = colorBuffer[v][u];
				}
#endif // #if HLSL_DEBUG_PIXEL == 1
			}
		}
	}

	if ( rasterCount == 0 )
		return c_pixelBackColor;

	return finalColor / rasterCount;
}

float3 testB( uint2 screenPos )
{	
	const uint pixelID = screenPos.y * c_pixelFrameSize.x + screenPos.x;
			
	const uint blockCellItemID = firstBlockCellItemIDs[pixelID];

	if ( blockCellItemID == 0 )
		return c_pixelBackColor;		

	return float4( 0.0f, 1.0f, 0.0f, 0.0f );
}

float3 testC( uint2 screenPos )
{
	const uint pixelID = screenPos.y * c_pixelFrameSize.x + screenPos.x;
		
	float minDepth = s_depthMax;
	uint minBlockCellItemID = (uint)-1;
	
	uint nextBlockCellItemID;
	for ( uint blockCellItemID = firstBlockCellItemIDs[pixelID]; blockCellItemID != 0; blockCellItemID = nextBlockCellItemID )
	{
		const BlockCellItem blockCellItem = blockCellItems[blockCellItemID];
		nextBlockCellItemID = blockCellItem.nextID;

		if ( blockCellItem.depth < minDepth )
		{
			minDepth = blockCellItem.depth;
			minBlockCellItemID = blockCellItemID;
		}
	}

	if (  minBlockCellItemID == (uint)-1 )
		return c_pixelBackColor;

	{
		const BlockCellItem blockCellItem = blockCellItems[minBlockCellItemID];
				
		const uint uOffset = ((blockCellItem.r8g8b8_u4v4 >> 4) & 0xF) - BUFFER_WIDTH/2;
		const uint vOffset = ((blockCellItem.r8g8b8_u4v4 >> 0) & 0xF) - BUFFER_WIDTH/2;		
		
		const uint mid = BUFFER_WIDTH/2;

		if ( c_pixelMode == 1 )
		{
			if ( uOffset == mid )
				return float3( 0.0f, 0.0f, 1.0f );

			if ( uOffset < mid )
				return float3( 1.0f, 0.0f, 0.0f );

			if ( uOffset > mid )
				return float3( 0.0f, 1.0f, 0.0f );
		}
		else
		{
			if ( vOffset == mid )
				return float3( 0.0f, 0.0f, 1.0f );

			if ( vOffset < mid )
				return float3( 1.0f, 0.0f, 0.0f );

			if ( vOffset > mid )
				return float3( 0.0f, 1.0f, 0.0f );
		}
	}

	return float3( 0.0f, 0.0f, 0.0f );
}

[ numthreads( 16, 16, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
#if HLSL_DEBUG_PIXEL == 1
	const bool debug = c_pixelDebug != 0 && c_pixelDebugCoords.x == DTid.x && c_pixelDebugCoords.y == DTid.y;
#endif // #if HLSL_DEBUG_PIXEL == 1

	const uint2 screenPos = uint2( DTid.x, c_pixelFrameSize.y - DTid.y - 1 );
	if ( c_pixelMode == 0)
		outputColors[DTid.xy] = float4( testA( screenPos, debug ), 1.0f );
	else
		outputColors[DTid.xy] = float4( testC( screenPos ), 1.0f );
}
