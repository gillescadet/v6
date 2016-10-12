#define HLSL
#include "player_shared.h"

SamplerState bilinearSampler : REGISTER_SAMPLER( HLSL_BILINEAR_SLOT );

Texture2D< float4 > leftColors : REGISTER_SRV( HLSL_LCOLOR_SLOT );
Texture2D< float4 > rightColors : REGISTER_SRV( HLSL_RCOLOR_SLOT );

RWTexture2D< float4 > surfaceColors : REGISTER_UAV( HLSL_SURFACE_SLOT );

float4 ComputeColor( uint2 pixelCoords, Texture2D< float4 > frameColors )
{
	const float2 uv = mad( pixelCoords + 0.5f, c_composeFrameUVScale, c_composeFrameUVBias );
	const float mask = all( saturate( uv ) == uv ) ? 1.0f : 0.0f;
	const float3 color = frameColors.SampleLevel( bilinearSampler, uv, 0.0f ).rgb;
	return float4( color * mask, 0.0f );
}

[numthreads(8, 8, 1)]
void main_surface_compose_cs( uint3 DTid : SV_DispatchThreadID )
{
	if ( DTid.x < c_composeSurfaceWidth )
		surfaceColors[DTid.xy] = ComputeColor( DTid.xy, leftColors );
	else
		surfaceColors[DTid.xy] = ComputeColor( uint2( DTid.x-c_composeSurfaceWidth, DTid.y ), rightColors );
}