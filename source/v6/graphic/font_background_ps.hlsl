#include "font.hlsli"

float4 main_font_background_ps( PixelInput i ) : SV_TARGET
{
	return float4( i.color.rgb * i.color.a, 0.8f );
}
