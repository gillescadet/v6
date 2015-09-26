#include "block_render.hlsli"

float4 main( PixelInput i ) : SV_TARGET
{
	return float4( i.color, 1.0 );
}