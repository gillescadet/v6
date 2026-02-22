#include "player_list.hlsli"

SamplerState trilinearSampler	: REGISTER_SAMPLER( HLSL_TRILINEAR_SLOT );

Texture2D texAlbedo				: REGISTER_SRV( HLSL_ALBEDO_SLOT );
Texture2D texOverlay			: REGISTER_SRV( HLSL_OVERLAY_SLOT );

float4 main_player_list_ps( PixelInput i ) : SV_TARGET
{
	if ( abs( i.uv.z - 0.0f ) < 0.01f )
	{
		return float4( texAlbedo.Sample( trilinearSampler, i.uv.xy ).rgb, 1.0f );
	}
	else if ( abs( i.uv.z - 1.0f ) < 0.01f )
	{
		const float3 color = texOverlay.Sample( trilinearSampler, i.uv.xy ).rgb;
		const float lum = color.x + color.y + color.z;
		return lum < 0.25f ? float4( 0.0f, 0.0f, 0.0f, lum ) : float4( color, 1.0f );
	}
	else
	{
		return c_listColor;
	}
}
