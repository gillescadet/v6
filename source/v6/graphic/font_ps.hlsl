#include "font.hlsli"

SamplerState trilinearSampler	: REGISTER_SAMPLER( HLSL_TRILINEAR_SLOT );

Texture2D texFont				: REGISTER_SRV( HLSL_FONT_TEXTURE_SLOT );

float4 main_font_ps( PixelInput i ) : SV_TARGET
{
	const float alpha = texFont.Sample( trilinearSampler, i.uv ).r * i.color.a;
	
	return float4( i.color.rgb * alpha, 0.0f );
	//return float4( float3( 1.0f, 0.0f, 0.0f ), 0.0f );
}
