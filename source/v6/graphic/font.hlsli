/*V6*/

#ifndef __V6_FONT_GENERIC_H__
#define __V6_FONT_GENERIC_H__

#define HLSL

#include "font_shared.h"

struct PixelInput
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
	float2 uv		: UV;
};

#endif // __V6_FONT_GENERIC_H__
