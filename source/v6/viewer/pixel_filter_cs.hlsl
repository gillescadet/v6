#define HLSL
#include "common_shared.h"

Texture2D< float4 > inputColors		: register( HLSL_COLOR_SRV );
Texture2D< float > inputDepths		: register( HLSL_DEPTH_SRV );

RWTexture2D< float4 > outputColors	: register( HLSL_PIXEL_COLOR_UAV );

[ numthreads( 16, 16, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
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
}
