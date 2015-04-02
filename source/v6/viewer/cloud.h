/*V6*/

#ifndef __V6_HLSL_CLOUD_H__
#define __V6_HLSL_CLOUD_H__

#define HLSL

#include "common_shared.h"

struct PixelInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

Texture2D<float4> colors : register( HLSL_COLOR_SRV );
Texture2D<float> depths : register( HLSL_DEPTH_SRV );

#endif // __V6_HLSL_CLOUD_H__
