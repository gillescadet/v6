#include "player_list.hlsli"

SamplerState trilinearSampler	: REGISTER_SAMPLER( HLSL_TRILINEAR_SLOT );

Texture2D texAlbedo				: REGISTER_SRV( HLSL_LIST_ALBEDO_SLOT );

float4 main_player_list_ps( PixelInput i ) : SV_TARGET
{
	const float3 albedo = texAlbedo.Sample( trilinearSampler, i.uv ).rgb;
	const float useAlbedo = all( saturate( i.uv ) == i.uv );
	return lerp( c_listColor, float4( albedo, 1.0f ), useAlbedo );
}
