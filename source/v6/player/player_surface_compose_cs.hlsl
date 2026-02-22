#define HLSL
#include "player_shared.h"

SamplerState bilinearSampler : REGISTER_SAMPLER( HLSL_BILINEAR_SLOT );

Texture2D< float4 > leftColors : REGISTER_SRV( HLSL_LCOLOR_SLOT );
Texture2D< float4 > rightColors : REGISTER_SRV( HLSL_RCOLOR_SLOT );

RWTexture2D< float4 > surfaceColors : REGISTER_UAV( HLSL_SURFACE_SLOT );

float Smoothstep( float x )
{
	return x * x * (3.0f - 2.0f * x);
}

float4 ComputeColor( uint2 pixelCoords, Texture2D< float4 > frameColors )
{
	const float2 uv = mad( pixelCoords + 0.5f, c_composeFrameUVScale, c_composeFrameUVBias );
	const float2 distanceToCenter = abs( uv * 2.0f - 1.0f );
	const float maxDistanceToCenter = max( distanceToCenter.x, distanceToCenter.y );
	const float4 frameColor = frameColors.SampleLevel( bilinearSampler, uv, 0.0f );
	const float fade = Smoothstep( saturate( (maxDistanceToCenter - 0.98f) * 50.0f ) );
	return lerp( frameColor, c_composeBackColor, fade );
}

[numthreads(8, 8, 1)]
void main_player_surface_compose_cs( uint3 DTid : SV_DispatchThreadID )
{
	if ( DTid.x < c_composeSurfaceWidth )
		surfaceColors[DTid.xy] = ComputeColor( DTid.xy, leftColors );
	else
		surfaceColors[DTid.xy] = ComputeColor( uint2( DTid.x-c_composeSurfaceWidth, DTid.y ), rightColors );
}