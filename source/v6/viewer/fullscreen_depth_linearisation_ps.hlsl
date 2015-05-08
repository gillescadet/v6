#define HLSL

#include "common_shared.h"

Texture2D<float> depths : register( HLSL_DEPTH_SRV );

float main( float4 pos : SV_Position ) : SV_TARGET
{
	const float z = rcp( depths.Load( int3( pos.x, pos.y, 0 ) ) * depthLinearScale + depthLinearBias );
	return z;
}