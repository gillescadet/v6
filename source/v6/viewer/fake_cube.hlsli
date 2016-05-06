/*V6*/

#ifndef __V6_HLSL_FAKE_CUBE_H__
#define __V6_HLSL_FAKE_CUBE_H__

#define HLSL

#include "viewer_shared.h"

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 color	: COLOR;
};

#endif // __V6_HLSL_FAKE_CUBE_H__
