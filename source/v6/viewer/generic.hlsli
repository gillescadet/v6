/*V6*/

#ifndef __V6_HLSL_GENERIC_H__
#define __V6_HLSL_GENERIC_H__

#define HLSL

#include "common_shared.h"

struct VertexInput
{
	float3 position : POSITION;
	float4 color	: COLOR;
	float2 uv		: USER0;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
	float2 uv		: USER0;
};

#endif // __V6_HLSL_GENERIC_H__
