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

#if HLSL_DEBUG_PIXEL == 1
static const uint s_pixelMode = c_pixelMode;
#else
static const uint s_pixelMode = 0;
#endif

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
				const float r = ((blockCellItem.r8g8b8a8 >> 24) & 0xFF) * inv255;
				const float g = ((blockCellItem.r8g8b8a8 >> 16) & 0xFF) * inv255;
				const float b = ((blockCellItem.r8g8b8a8 >> 8) & 0xFF) * inv255;
				const float3 otherColor = float3( r, g, b );
				
				const int uOffset = ((blockCellItem.u8v8w8h8 >> 24) & 0xF) - BUFFER_WIDTH/2;
				const int vOffset = ((blockCellItem.u8v8w8h8 >> 16) & 0xF) - BUFFER_WIDTH/2;

#if HLSL_DEBUG_PIXEL == 1
				const uint layer = pixelDebugBuffer[0].points[j+1][i+1].layerCount;
				if ( debug && layer < 4 )
				{				
					PixelDebugLayer debugLayer;
					debugLayer.colorAndDepth = float4( otherColor, blockCellItem.depth );
					debugLayer.uv = float2( uOffset, vOffset );
					debugLayer.packedID = blockCellItem.packedID;

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

	return float3( 0.0f, 1.0f, 0.0f );
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
				
		const uint uOffset = ((blockCellItem.u8v8w8h8 >> 24) & 0xF) - BUFFER_WIDTH/2;
		const uint vOffset = ((blockCellItem.u8v8w8h8 >> 16) & 0xF) - BUFFER_WIDTH/2;		
		
		const uint mid = BUFFER_WIDTH/2;

		if ( s_pixelMode == 1 )
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
				const float r = ((blockCellItem.r8g8b8a8 >> 24) & 0xFF) * inv255;
				const float g = ((blockCellItem.r8g8b8a8 >> 16) & 0xFF) * inv255;
				const float b = ((blockCellItem.r8g8b8a8 >> 8) & 0xFF) * inv255;
				const float3 otherColor = float3( r, g, b );
				
				const float norm = 1.0f / 255.0f;
				const float uRelative = ((blockCellItem.u8v8w8h8 >> 24) & 0xFF) * norm;
				const float vRelative = ((blockCellItem.u8v8w8h8 >> 16) & 0xFF) * norm;
				const float hRelative = ((blockCellItem.u8v8w8h8 >>  0) & 0xFF) * norm * 0.95f;
				const float wRelative = ((blockCellItem.u8v8w8h8 >>  8) & 0xFF) * norm * 0.95f;

				const float2 posRelative = float2( i + uRelative, j + vRelative );
				const int2 posMin = int2( floor( float2( (posRelative.x - wRelative) * BUFFER_WIDTH, (posRelative.y - hRelative) * BUFFER_WIDTH ) ) );
				const int2 posMax = int2( floor( float2( (posRelative.x + wRelative) * BUFFER_WIDTH, (posRelative.y + hRelative) * BUFFER_WIDTH ) ) );
				const int uMin = max( posMin.x, 0 );
				const int vMin = max( posMin.y, 0 );
				const int uMax = min( posMax.x, BUFFER_WIDTH-1 );
				const int vMax = min( posMax.y, BUFFER_WIDTH-1 );

#if HLSL_DEBUG_PIXEL == 1
				const uint layer = pixelDebugBuffer[0].points[j+1][i+1].layerCount;
				if ( debug && layer < 4 )
				{				
					PixelDebugLayer debugLayer;
					debugLayer.colorAndDepth = float4( otherColor, blockCellItem.depth );
					debugLayer.uv = float2( uRelative, vRelative );
					debugLayer.wh = float2( wRelative, hRelative );
#if 0
					debugLayer.uvMin = posMin;//int2( uMin, vMin );
					debugLayer.uvMax = posMax;//int2( uMax, vMax );
#endif
					debugLayer.packedID = blockCellItem.packedID;

					pixelDebugBuffer[0].points[j+1][i+1].layers[layer] = debugLayer;
					++pixelDebugBuffer[0].points[j+1][i+1].layerCount;
				}
#endif // #if HLSL_DEBUG_PIXEL == 1				

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
	// return finalColor / 16.0f;
}

float3 testAlignedQuadB( uint2 screenPos )
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
				
		const float norm = 1.0f / 255.0f;
		const float u = ((blockCellItem.u8v8w8h8 >> 24) & 0xFF) * norm - 0.5f;
		const float v = ((blockCellItem.u8v8w8h8 >> 16) & 0xFF) * norm - 0.5f;
		const float w = ((blockCellItem.u8v8w8h8 >>  8) & 0xFF) * norm;
		const float h = ((blockCellItem.u8v8w8h8 >>  0) & 0xFF) * norm;
		
		if ( s_pixelMode == 1 )
		{
			if ( abs( u ) > 0.5f )
				return float3( 0.0f, 1.0f, 1.0f );
			else if ( u < 0.0f )
				return float3( 1.0f, 0.0f, 0.0f );
			else
				return float3( 0.0f, 1.0f, 0.0f );
		}
		
		if ( s_pixelMode == 2 )
		{
			if ( abs( v ) > 0.5f )
				return float3( 0.0f, 1.0f, 1.0f );
			if ( v < 0.0f )
				return float3( 1.0f, 0.0f, 0.0f );
			else
				return float3( 0.0f, 1.0f, 0.0f );
		}

		if ( s_pixelMode == 3 )
		{
			if ( w < 0.50f )
				return float3( 0.0f, 0.0f, 1.0f );
			else if ( w < 0.67f )
				return float3( 0.0f, 1.0f, 1.0f );
			else if ( w < 0.84f )
				return float3( 0.0f, 1.0f, 0.0f );
			else if ( w < 1.00f )
				return float3( 1.0f, 1.0f, 0.0f );
			else
				return float3( 1.0f, 0.0f, 0.0f );
		}

		if ( s_pixelMode == 4 )
		{
			if ( h < 0.50f )
				return float3( 0.0f, 0.0f, 1.0f );
			else if ( h < 0.67f )
				return float3( 0.0f, 1.0f, 1.0f );
			else if ( h < 0.84f )
				return float3( 0.0f, 1.0f, 0.0f );
			else if ( h < 1.00f )
				return float3( 1.0f, 1.0f, 0.0f );
			else
				return float3( 1.0f, 0.0f, 0.0f );
		}
	}

	return float3( 0.0f, 0.0f, 0.0f );
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

#if HLSL_TRACE_USE_ALIGNED_QUAD == 1
	if ( s_pixelMode == 0)
		outputColors[DTid.xy] = float4( testAlignedQuadA( screenPos, debug ), 1.0f );
	else
		outputColors[DTid.xy] = float4( testAlignedQuadB( screenPos ), 1.0f );
#else // #if HLSL_TRACE_USE_ALIGNED_QUAD == 1
	if ( s_pixelMode == 0)
		outputColors[DTid.xy] = float4( testA( screenPos, debug ), 1.0f );
	else
		outputColors[DTid.xy] = float4( testC( screenPos ), 1.0f );
#endif // #if HLSL_TRACE_USE_ALIGNED_QUAD != 1
}
