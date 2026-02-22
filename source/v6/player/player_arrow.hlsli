/*V6*/

#ifndef __V6_HLSL_ARROW_H__
#define __V6_HLSL_ARROW_H__

#define HLSL

#include "player_shared.h"

struct VertexInput
{
	float3 position : POSITION;
};

struct PixelInput
{
	float4 position : SV_POSITION;
};

#endif // __V6_HLSL_ARROW_H__
