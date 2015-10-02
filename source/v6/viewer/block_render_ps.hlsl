#include "block_render.hlsli"

float4 main( PixelInput i ) : SV_TARGET
{
	return i.color;
}