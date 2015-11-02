#include "generic.hlsli"

SamplerState trilinearSampler	: register( HLSL_TRILINEAR_SAMPLER );

Texture2D texAlbedo				: register( HLSL_GENERIC_ALBEDO_SRV );
Texture2D texAlpha				: register( HLSL_GENERIC_ALPHA_SRV );

static const float3 s_sunDir = normalize( float3( 0.8f, 1.0f, 0.5f ) );
static const float3 s_skyColor = float3( 0.1f, 0.3f, 0.75f );
static const float3 s_sunColor = float3( 1.0f, 0.75f, 0.2f ) * 2.0f;

float4 main( PixelInput i ) : SV_TARGET
{
	const float alpha = c_genericUseAlpha ? texAlpha.Sample( trilinearSampler, i.uv ).r : 1.0f;
	
#if USE_ALPHA_TEST == 1
	if ( alpha < 0.5f )
		discard;
#endif

	const float3 albedo = c_genericUseAlbedo ? texAlbedo.Sample( trilinearSampler, i.uv ).rgb : float3( 1.0f, 1.0f, 1.0f);	
	
	const float3 ambientLight = float3( 0.4f, 0.4f, 0.4f );
	const float3 skyLight = max( 0.0f, i.normal.y ) * s_skyColor;	
	const float3 sunLight = max( 0.0f, dot( i.normal, s_sunDir ) ) * s_sunColor;	

	const float3 color = (albedo * c_genericDiffuse) * (ambientLight + skyLight + sunLight);
	// const float3 color = (ambientLight + skyLight + sunLight);

	return float4( color, alpha );
}
