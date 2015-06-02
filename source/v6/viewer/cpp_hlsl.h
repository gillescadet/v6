/*V6*/

#ifndef __V6_HLSL_CPP_HLSL_H__
#define __V6_HLSL_CPP_HLSL_H__

#ifdef HLSL

#define BEGIN_V6_HLSL_NAMESPACE
#define END_V6_HLSL_NAMESPACE

#define CBUFFER( NAME, SLOT ) cbuffer NAME : register( b##SLOT )

#else

#include <v6/core/vec3.h>
#include <v6/core/vec4.h>
#include <v6/core/mat4x4.h>

#define BEGIN_V6_HLSL_NAMESPACE		namespace v6 { namespace hlsl {
#define END_V6_HLSL_NAMESPACE		} }

#define CBUFFER( NAME, SLOT ) static const uint NAME##Slot = SLOT; struct NAME

#define row_major

typedef unsigned int uint;

struct uint2
{
	uint x;
	uint y;
};

struct uint3
{
	uint x;
	uint y;
	uint z;
};

struct uint4
{
	uint x;
	uint y;
	uint z;
	uint w;
};

struct int2
{
	int x;
	int y;
};

struct int3
{
	int x;
	int y;
	int z;
};

struct int4
{
	int x;
	int y;
	int z;
	int w;
};

struct float2
{
	float x;
	float y;
};

typedef v6::core::Vec3		float3;
typedef v6::core::Vec4		float4;
typedef v6::core::Mat4x4	matrix;

#endif

#endif // __V6_HLSL_CPP_HLSL_H__