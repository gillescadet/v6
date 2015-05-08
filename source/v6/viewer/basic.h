/*V6*/

#ifndef __V6_HLSL_BASIC_H__
#define __V6_HLSL_BASIC_H__

#define HLSL

#include "common_shared.h"

struct VertexInput
{
	float3 position : POSITION;
	float4 color : COLOR;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

#endif // __V6_HLSL_BASIC_H__
