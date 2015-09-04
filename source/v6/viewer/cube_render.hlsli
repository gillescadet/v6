/*V6*/

#ifndef __V6_HLSL_CUBE_RENDER_H__
#define __V6_HLSL_CUBE_RENDER_H__

#define HLSL

#include "common_shared.h"

Texture2DArray< float4 > colors		: register( HLSL_COLOR_SRV );

struct VertexInput
{
	float3 position				: POSITION;
	float2 uv					: USER0;
};

struct PixelInput
{
	float4 position				: SV_POSITION;
	float2 uv					: UV;
	nointerpolation uint faceID	: FACEID;
};

#endif // __V6_HLSL_CUBE_RENDER_H__
