#define HLSL
#include "player_shared.h"

Texture2D< float4 > leftColors : REGISTER_SRV( HLSL_LCOLOR_SLOT );
Texture2D< float4 > rightColors : REGISTER_SRV( HLSL_RCOLOR_SLOT );

RWTexture2D< float4 > surfaceColors : REGISTER_UAV( HLSL_SURFACE_SLOT );

float4 ComputeColor( uint2 pixelCoords, Texture2D< float4 > frameColors )
{
	float3 color = frameColors[pixelCoords.xy].rgb;

#if 0
	const float2 uv = pixelCoords * c_composeFrameInvSize;
	const float2 clip = uv * 2.0f - 1.0f;
	
	color += float3( 0.0f, 1.0f, 0.0f ) * (dot( clip, clip ) >= 1.0f );
#endif
	return float4( color, 0.0f );
}

[numthreads(8, 8, 1)]
void main_surface_compose_cs( uint3 DTid : SV_DispatchThreadID )
{
	if ( DTid.x < c_composeFrameWidth )
		surfaceColors[DTid.xy] = ComputeColor( DTid.xy, leftColors );
	else
		surfaceColors[DTid.xy] = ComputeColor( uint2( DTid.x-c_composeFrameWidth, DTid.y ), rightColors );
}