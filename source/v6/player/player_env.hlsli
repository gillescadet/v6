/*V6*/

#ifndef __V6_HLSL_ENV_H__
#define __V6_HLSL_ENV_H__

#define HLSL

#include "player_shared.h"

struct VertexInput
{
	float3 position : POSITION;
	float2 uv		: USER0;
};

struct PixelInput
{
	float4 position : SV_POSITION;
	float2 uv		: USER0;
};

#endif // __V6_HLSL_ENV_H__
