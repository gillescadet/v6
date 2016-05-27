#include "viewer_basic.hlsli"

float4 main_viewer_basic_ps( PixelInput i ) : SV_TARGET
{
#if 1
	return i.color;
#else
	return float4(
		( ( (uint)i.position.x << 5) & 255 ) / 255.0f,
		( ( (uint)i.position.y << 5) & 255 ) / 255.0f,
		0.0f,
		1.0f );
#endif
}