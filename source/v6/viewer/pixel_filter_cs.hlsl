#define HLSL
#include "common_shared.h"

Texture2D< float4 > inputColors		: register( HLSL_COLOR_SRV );
Texture2D< float > inputDepths		: register( HLSL_DEPTH_SRV );

RWTexture2D< float4 > outputColors	: register( HLSL_PIXEL_COLOR_UAV );

[ numthreads( 16, 16, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float3 otherColors[8];
	float otherDepths[8];
	float otherWeigths[8];
	uint otherCount = 0;
	float minDepth = 1.0f;
	uint minMip = HLSL_MIP_MAX_COUNT-1;
	float4 finalColor = float4( 0.0f, 0.0f, 0.0f, 0.0f );

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
				otherWeigths[otherCount] = 1.0f / float( 1 << (i * i + j * j) );
				otherCount++;

				if ( otherDepth < minDepth )
				{
					minDepth = otherDepth;
					minMip = otherMip;
				}
			}				
		}
	}

	if ( otherCount > 0 )
	{
		const float minZ = 1.0f / ( mad ( minDepth, c_pixelDepthLinearScale, c_pixelDepthLinearBias ) );
		const float invCellSize = c_pixelInvCellSizes[minMip].x;
		for ( uint otherID = 0; otherID < otherCount; ++otherID )
		{
			const float otherZ = rcp( ( mad ( otherDepths[otherID], c_pixelDepthLinearScale, c_pixelDepthLinearBias ) ) );
			const float normalizedZ = 1.0f + (otherZ - minZ) * invCellSize;
			const float w = rcp( normalizedZ * normalizedZ ) * otherWeigths[otherID];
			finalColor.rgb += w * otherColors[otherID];
			finalColor.a += w;
		}

		finalColor /= finalColor.a;
	}

	outputColors[DTid.xy] = finalColor;
}
