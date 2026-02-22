#include "player_env.hlsli"

SamplerState trilinearSampler	: REGISTER_SAMPLER( HLSL_TRILINEAR_SLOT );

Texture2D texAlbedo				: REGISTER_SRV( HLSL_ALBEDO_SLOT );

float4 main_player_env_ps( PixelInput i ) : SV_TARGET
{
	return float4( texAlbedo.Sample( trilinearSampler, i.uv.xy ).rgb, 1.0f );
}
