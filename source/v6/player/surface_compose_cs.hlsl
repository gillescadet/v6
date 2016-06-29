#define HLSL
#include "player_shared.h"

Texture2D< float4 > leftColors : REGISTER_SRV( HLSL_LCOLOR_SLOT );
Texture2D< float4 > rightColors : REGISTER_SRV( HLSL_RCOLOR_SLOT );

RWTexture2D< float4 > surfaceColors : REGISTER_UAV( HLSL_SURFACE_SLOT );

float4 ComputeColor( uint2 pixelCoords, Texture2D< float4 > frameColors )
{
	return float4( frameColors[pixelCoords.xy].rgb, 0.0f );
}

[numthreads(8, 8, 1)]
void main_surface_compose_cs( uint3 DTid : SV_DispatchThreadID )
{
	if ( DTid.x < c_composeFrameWidth )
		surfaceColors[DTid.xy] = ComputeColor( DTid.xy, leftColors );
	else
		surfaceColors[DTid.xy] = ComputeColor( uint2( DTid.x-c_composeFrameWidth, DTid.y ), rightColors );
}