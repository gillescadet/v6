#include "generic.hlsli"

SamplerState trilinearSampler	: register( HLSL_TRILINEAR_SAMPLER );

Texture2D texAlbedo				: register( HLSL_GENERIC_ALBEDO_SRV );

float4 main( PixelInput i ) : SV_TARGET
{
	const float3 albedo = c_genericUseAlbedo ? texAlbedo.Sample( trilinearSampler, i.uv ).rgb : i.color.rgb;

	return float4( albedo, 1.0f );
}
