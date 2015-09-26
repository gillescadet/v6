#include "cube_render.hlsli"

float4 main( PixelInput i ) : SV_TARGET
{		
	const int4 coords = int4( i.uv.x * c_frameWidth, i.uv.y * c_frameHeight, i.faceID, 0 );
	const float3 cubeColor = colors.Load( coords ).rgb;
	return float4( cubeColor, 1.0 );
}