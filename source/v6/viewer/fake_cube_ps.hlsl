#include "fake_cube.hlsli"

float4 main_fake_cube_ps( PixelInput i ) : SV_TARGET
{		
	return float4( i.color, 1.0 );
}