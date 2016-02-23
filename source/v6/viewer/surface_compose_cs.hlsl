#define HLSL
#include "common_shared.h"

Texture2D< float4 > leftColors : register( HLSL_LCOLOR_SRV );
Texture2D< float4 > rightColors : register( HLSL_RCOLOR_SRV );

RWTexture2D< float4 > surfaceColors : register( HLSL_SURFACE_UAV );

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if ( DTid.x < c_composeFrameWidth )
		surfaceColors[DTid.xy] = leftColors[DTid.xy];
	else
		surfaceColors[DTid.xy] = rightColors[uint2( DTid.x-c_composeFrameWidth, DTid.y)];
}