/*V6*/

#ifndef __V6_CORE_CPP_HLSL_H__
#define __V6_CORE_CPP_HLSL_H__

#ifdef HLSL

#define V6_CPP 0

#define BEGIN_V6_HLSL_NAMESPACE
#define END_V6_HLSL_NAMESPACE

#define CBUFFER( NAME, SLOT )				cbuffer NAME : register( b##SLOT )
#define TYPEDBUFFER( NAME, TYPE, SLOT )		Buffer< TYPE > NAME : register( t##SLOT )

#define DEFINE( NAME )
#define OUTPUT( TYPE )						out TYPE

#else // #ifdef HLSL

#define V6_CPP 1

#include <v6/core/vec2.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>
#include <v6/core/vec4.h>
#include <v6/core/vec4i.h>
#include <v6/core/mat4x4.h>

#define BEGIN_V6_HLSL_NAMESPACE				namespace v6 { namespace hlsl {
#define END_V6_HLSL_NAMESPACE				} }

BEGIN_V6_HLSL_NAMESPACE

#define CBUFFER( NAME, SLOT )				static const uint NAME##Slot = SLOT; struct NAME
#define TYPEDBUFFER( NAME, TYPE, SLOT )		TYPE* NAME

#define row_major

#define OUTPUT( TYPE )						TYPE&

typedef unsigned int uint;

typedef v6::Vec2i		int2;
typedef v6::Vec4i		int4;

typedef v6::Vec2u		uint2;
typedef v6::Vec3u		uint3;
typedef v6::Vec4u		uint4;

typedef v6::Vec2		float2;
typedef v6::Vec3		float3;
typedef v6::Vec4		float4;
typedef v6::Mat4x4	matrix;

struct int3 : Vec3i
{
	int3() {}
	int3( const Vec3i& v )
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}
	int3( int vx, int vy, int vz )
	{
		x = vx;
		y = vy;
		z = vz;
	}
};

END_V6_HLSL_NAMESPACE

#endif // #ifdef HLSL

#endif // __V6_CORE_CPP_HLSL_H__