#define HLSL
#include "common_shared.h"

#define BUFFER_WIDTH	HLSL_PIXEL_SUPER_SAMPLING_WIDTH

Texture2D< float4 > inputColors							: register( HLSL_COLOR_SRV );
Texture2D< float2 > inputUVs							: register( HLSL_UV_SRV );
Texture2D< float > inputDepths							: register( HLSL_DEPTH_SRV );

RWTexture2D< float4 > outputColors						: register( HLSL_PIXEL_COLOR_UAV );
#if HLSL_DEBUG_PIXEL == 1
RWStructuredBuffer< PixelDebugBuffer > pixelDebugBuffer	: register( HLSL_PIXEL_DEBUG_UAV );
#endif // #if HLSL_DEBUG_PIXEL == 1

#if HLSL_DEBUG_PIXEL == 1
static const uint s_pixelMode = c_pixelMode;
#else
static const uint s_pixelMode = 0;
#endif

float3 testA( uint2 screenPos )
{
#if HLSL_DEBUG_PIXEL == 1
	const bool debug = c_pixelDebug != 0 && c_pixelDebugCoords.x == screenPos.x && c_pixelDebugCoords.y == screenPos.y;
#endif // #if HLSL_DEBUG_PIXEL == 1

	float depthBuffer[BUFFER_WIDTH][BUFFER_WIDTH];
	float3 colorBuffer[BUFFER_WIDTH][BUFFER_WIDTH];

	{
		for ( uint v = 0; v < BUFFER_WIDTH; ++v )
		{
			for ( uint u = 0; u < BUFFER_WIDTH; ++u )
			{
				depthBuffer[v][u] = 1.0f;
				colorBuffer[v][u] = float3( 0.0f, 0.0f, 0.0f );
			}
		}
	}
	
	float4 aroundColor = float4( 0.0f, 0.0f, 0.0f, 0.0f );
		
	for ( int j = -1; j <= 1; ++j )
	{
		for ( int i = -1; i <= 1; ++i )
		{
			const int3 otherCoords = int3( screenPos.x + i, screenPos.y + j, 0 );
			const float4 otherColor = inputColors.Load( otherCoords );

#if HLSL_DEBUG_PIXEL == 1
			if ( debug )
			{				
				PixelDebugPoint debugPoint = (PixelDebugPoint)0;
				debugPoint.layers[0].color = inputColors.Load( otherCoords );
				debugPoint.layers[0].depth = inputDepths.Load( otherCoords );
				debugPoint.layers[0].uv = inputUVs.Load( otherCoords );
				debugPoint.layerCount = 1;

				pixelDebugBuffer[0].points[j+1][i+1] = debugPoint;
			}
#endif // #if HLSL_DEBUG_PIXEL == 1

			if ( otherColor.a == 0.0f )
				continue;

			const float otherDepth = inputDepths.Load( otherCoords );
			const float2 otherUV = float2( i, -j ) + inputUVs.Load( otherCoords ) * 0.5f;

			const int2 pMin = int2( mad( otherUV, BUFFER_WIDTH, 2 * BUFFER_WIDTH ) ) - 2 * BUFFER_WIDTH;
			const int uMin = max( pMin.x, 0 );
			const int vMin = max( pMin.y, 0 );
			const int uMax = min( pMin.x+BUFFER_WIDTH, BUFFER_WIDTH-1 );
			const int vMax = min( pMin.y+BUFFER_WIDTH, BUFFER_WIDTH-1 );

			if ( i != 0 || j != 0 )
			{
				const float aroundWeight = rcp( dot( otherUV, otherUV ) );
				aroundColor.rgb += otherColor.rgb * aroundWeight;
				aroundColor.a += aroundWeight;
			}
						
			for ( int v = vMin; v <= vMax; ++v )
			{
				for ( int u = uMin; u <= uMax; ++u )
				{
					if ( otherDepth < depthBuffer[v][u] )
					{
						depthBuffer[v][u] = otherDepth;
						colorBuffer[v][u] = otherColor.rgb;
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
				rasterCount += depthBuffer[v][u] < 1.0f ? 1.0f : 0.0f;

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

	if ( s_pixelMode == 1 )
	{
		if ( rasterCount == BUFFER_WIDTH * BUFFER_WIDTH )	
			return float3( 0.5f, 0.5f, 0.5f );			
		
		if ( rasterCount >= 0.75f * BUFFER_WIDTH * BUFFER_WIDTH )	
			return float3( 0.0f, 1.0f, 0.0f );			
		
		if ( rasterCount >= 0.50f * BUFFER_WIDTH * BUFFER_WIDTH )	
			return float3( 0.0f, 0.0f, 1.0f );			
		
		if ( rasterCount >= 0.25f * BUFFER_WIDTH * BUFFER_WIDTH )	
			return float3( 1.0f, 0.0f, 1.0f );
		
		if ( rasterCount >= 1 )	
			return float3( 1.0f, 0.0f, 0.0f );
		
		return float3( 0.0f, 0.0f, 0.0f );
	}

	if ( s_pixelMode == 2 )
		return finalColor / 16.0f;

	if ( rasterCount == 0 )
	{
#if 0
		if ( aroundColor.a > 0.0f )
			return aroundColor.rgb / aroundColor.a;
#endif
		return c_pixelBackColor;		
	}	

	return finalColor / rasterCount;
}

float3 testC( uint2 screenPos )
{
	const int3 centerCoords = int3( screenPos.x, screenPos.y, 0 );
	const float2 centerUV = inputUVs.Load( centerCoords );
		
	if ( s_pixelMode == 3 )
	{
		if ( centerUV.x < 0.0f )
			return float3( 1.0f, 0.0f, 0.0f );

		if ( centerUV.x > 0.0f )
			return float3( 0.0f, 1.0f, 0.0f );
	}
	else
	{
		if ( centerUV.y < 0.0f )
			return float3( 1.0f, 0.0f, 0.0f );

		if ( centerUV.y > 0.0f )
			return float3( 0.0f, 1.0f, 0.0f );
	}

	return float3( 0.0f, 0.0f, 0.0f );
}

void testD( uint2 screenPos )
{
	float3 otherColors[9];
	float otherDepths[9];
	float otherWeigths[9];
	uint otherCount = 0;
	uint centerCount = 0;
	float centerDepth = 1.0f;
	
	float minDepth = 1.0f;
	float maxDepth = 0.0f;
	uint minMip = HLSL_MIP_MAX_COUNT-1;		
	
	float3 centerColor = float3( 0.0f, 0.0f, 0.0f );	

	for ( int j = -1; j <= 1; ++j )
	{
		for ( int i = -1; i <= 1; ++i )
		{
			const int3 otherCoords = int3( screenPos.x + i, screenPos.y + j, 0 );
			const float4 otherColor = inputColors.Load( otherCoords );
			if ( otherColor.a > 0.0f )
			{
				const uint otherMip = uint( otherColor.a * float( HLSL_MIP_MAX_COUNT ) - 0.5f );
				const float otherDepth = inputDepths.Load( otherCoords );
					
				otherColors[otherCount] = otherColor.rgb;
				otherDepths[otherCount] = otherDepth;
				otherCount++;

				if ( otherDepth < minDepth )
				{
					minDepth = otherDepth;					
					minMip = otherMip;
				}

				maxDepth = max( maxDepth, otherDepth );					
				
				if ( i == 0 && j == 0 )
				{
					centerColor = otherColor.rgb;
					centerCount = 1;
				}
			}
		}
	}

	float4 finalColor = float4( 0.0f, 0.0f, 0.0f, 0.0f );

#if 0
	if ( otherCount > 0 )
	{
		const float invCellSize = c_pixelInvCellSizes[minMip].x;
		const float minZ = 1.0f / ( mad ( minDepth, c_pixelDepthLinearScale, c_pixelDepthLinearBias ) );
		const float maxZ = 1.0f / ( mad ( maxDepth, c_pixelDepthLinearScale, c_pixelDepthLinearBias ) );
		const float zRatio = (maxZ - minZ) * invCellSize;
		
		for ( uint otherID = 0; otherID < otherCount; ++otherID )
		{
			const float otherZ = rcp( ( mad ( otherDepths[otherID], c_pixelDepthLinearScale, c_pixelDepthLinearBias ) ) );
			const float normalizedZ = 1.0f + ( otherZ - minZ ) * invCellSize;
			const float depthWeigth = rcp( normalizedZ * normalizedZ );
			const float w = lerp( depthWeigth, 1.0f, zRatio / 32.0f ) * otherWeigths[otherID];
			finalColor.rgb += w * otherColors[otherID];
			finalColor.a += w;
		}

		finalColor /= finalColor.a;
	}
#endif
	
	//const float zRatio = (maxZ - minZ) * invCellSize;

	if ( centerCount == 0 )
		finalColor = float4( 0.0f, 1.0f, 0.0f, 1.0f );
	else
	{
		
	}


	outputColors[screenPos.xy] = finalColor;
}

void testE()
{
#if 0
	const float weigths[3][3] = 
	{
		{ 1.0f, 2.0f, 1.0f },
		{ 2.0f, 64.0f, 2.0f }, 
		{ 1.0f, 2.0f, 1.0f }
	};
	const uint pos[3][3] = 
	{
		{  6,  5,  4 },
		{  7, -1,  3 }, 
		{  0,  1,  2 }
	};
	float3 otherColors[8];
	float otherDepths[8];
	float otherWeigths[8];
	uint otherCount = 0;
	uint centerCount = 0;
	float minDepth = 1.0f;
	uint minMip = HLSL_MIP_MAX_COUNT-1;	
	float4 centerColor = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	
	uint lastPos = -1;
	uint isBorder = true;

	for ( int j = -1; j <= 1; ++j )
	{
		for ( int i = -1; i <= 1; ++i )
		{
			const int3 otherCoords = int3( DTid.x + i, DTid.y + j, 0 );
			const float4 otherColor = inputColors.Load( otherCoords );
			if ( otherColor.a > 0.0f )
			{
				const uint otherMip = uint( otherColor.a * float( HLSL_MIP_MAX_COUNT ) - 0.5f );
				const float otherDepth = inputDepths.Load( otherCoords );
					
				otherColors[otherCount] = otherColor.rgb;
				otherDepths[otherCount] = otherDepth;
				otherWeigths[otherCount] = weigths[i+1][j+1];				
				otherCount++;

				if ( otherDepth < minDepth )
				{
					minDepth = otherDepth;
					minMip = otherMip;
				}
				
				const bool isCenter = i == 0 && j == 0;
				centerCount += isCenter ? 1 : 0;

				const uint otherPos = pos[i+1][j+1];
				isBorder |= isCenter | (lastPos == -1) | (((lastPos+1) % 8) == otherPos) | (lastPos == ((otherPos+1) % 8));
				lastPos = isCenter ? lastPos : otherPos;

				centerColor = isCenter ? otherColor : centerColor;
			}				
		}
	}
		
	const uint aroundCount = otherCount - centerCount;

	float4 finalColor = centerColor;

	//if ( aroundCount > 3 || (aroundCount > 1 && !isBorder) )	
	{
		const float minZ = 1.0f / ( mad ( minDepth, c_pixelDepthLinearScale, c_pixelDepthLinearBias ) );
		const float invCellSize = c_pixelInvCellSizes[minMip].x;
		finalColor = float4( 0.0f, 0.0f, 0.0f, 0.0f );
		for ( uint otherID = 0; otherID < otherCount; ++otherID )
		{
			const float otherZ = rcp( ( mad ( otherDepths[otherID], c_pixelDepthLinearScale, c_pixelDepthLinearBias ) ) );
			const float normalizedZ = 1.0f + ( otherZ - minZ ) * invCellSize;
			const float w = rcp( normalizedZ * normalizedZ ) * otherWeigths[otherID];
			finalColor.rgb += w * otherColors[otherID];
			finalColor.a += w;
		}

		finalColor /= finalColor.a;
	}

	outputColors[DTid.xy] = finalColor;

#endif
}

uint CountConsecutiveBitsInAByte( uint val )
{
	uint shortVal = val | val << 8;
	uint c = 0;
	uint lastVal = 0;

	while ( shortVal != 0 )
	{
	    lastVal = shortVal;
		shortVal &= shortVal >> 1;
		++c;
	}

	const uint lowMask = ~(lastVal-1);
	const uint highMask = (lastVal << c)-1;
	const uint shortMask = highMask & lowMask;
	const uint byteMask = (shortMask & 0xFF) | (shortMask >> 8);
	
	if ( (val & byteMask) == val )
		return c;

	return (uint)-1;
}

float3 testF( uint2 screenPos )
{
	const uint borderBits[3][3] = 
	{
		{  6,  5,  4 },
		{  7, -1,  3 }, 
		{  0,  1,  2 }
	};

	float minDepth = 1.0f;
	uint borderMask = 0;

	for ( int j = -1; j <= 1; ++j )
	{
		for ( int i = -1; i <= 1; ++i )
		{
			const int3 otherCoords = int3( screenPos.x + i, screenPos.y + j, 0 );
			const float4 otherColor = inputColors.Load( otherCoords );
			if ( otherColor.a > 0.0f )
			{
				const float otherDepth = inputDepths.Load( otherCoords );
					
				if ( otherDepth < minDepth )
					minDepth = otherDepth;

				if ( i == 0 && j == 0 )
					continue;
			
				const uint borderBit = borderBits[i+1][j+1];
				borderMask |= 1 << borderBit;
			}				
		}
	}
	
	if ( CountConsecutiveBitsInAByte( borderMask ) <= 3 )
		return float3( 0.0f, 1.0f, 0.0f );
		
	return float3( 0.0f, 0.0f, 0.0f );
}

[ numthreads( 16, 16, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{	
	switch( s_pixelMode )
	{
	case 0:
	case 1:
	case 2:
		outputColors[DTid.xy] = float4( testA( DTid.xy ), 1.0f );
		break;
	case 3:
	case 4:
		outputColors[DTid.xy] = float4( testC( DTid.xy ), 1.0f );
		break;
	case 5:
		outputColors[DTid.xy] = float4( testF( DTid.xy ), 1.0f );
		break;
	}
}
