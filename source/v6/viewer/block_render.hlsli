/*V6*/

#ifndef __V6_HLSL_GRID_RENDER_H__
#define __V6_HLSL_GRID_RENDER_H__

#define HLSL

#include "common_shared.h"

struct PixelInput
{
					float4 position	: SV_POSITION;
	nointerpolation float4 color	: COLOR;
	nointerpolation float2 uv		: UV;
};

#endif // __V6_HLSL_GRID_RENDER_H__
