/*V6*/

#ifndef __V6_HLSL_GENERIC_H__
#define __V6_HLSL_GENERIC_H__

#define HLSL

#include "common_shared.h"

struct VertexInput
{
	float3 position : POSITION;
	float3 normal	: USER0;
	float2 uv		: USER1;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 normal	: NORMAL;
	float2 uv		: UV;
};

#endif // __V6_HLSL_GENERIC_H__
