/*V6*/

#pragma once

#ifndef __V6_CORE_MAT4X4_H__
#define __V6_CORE_MAT4X4_H__

#include <v6/core/math.h>
#include <v6/core/vec4.h>

BEGIN_V6_CORE_NAMESPACE

struct Mat4x4
{
public:
	union
	{
		struct
		{
			Vec4 m_row0;
			Vec4 m_row1;
			Vec4 m_row2;
			Vec4 m_row3;
		};
		Vec4 m_rows[4];
	};	

};

V6_INLINE void Mat4x4_Mul( Mat4x4* r, const Mat4x4& a, const Mat4x4& b )
{
	// left to right order
	auto __Dot = [ r, a, b ]( uint raw, uint col ) -> float
	{
		return
			b.m_rows[raw].x * a.m_row0.m_fValues[col] +
			b.m_rows[raw].y * a.m_row1.m_fValues[col] +
			b.m_rows[raw].z * a.m_row2.m_fValues[col] +
			b.m_rows[raw].w * a.m_row3.m_fValues[col];
	};

	r->m_row0 = Vec4_Make( __Dot( 0, 0 ), __Dot( 0, 1 ), __Dot( 0, 2 ), __Dot( 0, 3 ) );
	r->m_row1 = Vec4_Make( __Dot( 1, 0 ), __Dot( 1, 1 ), __Dot( 1, 2 ), __Dot( 1, 3 ) );
	r->m_row2 = Vec4_Make( __Dot( 2, 0 ), __Dot( 2, 1 ), __Dot( 2, 2 ), __Dot( 2, 3 ) );
	r->m_row3 = Vec4_Make( __Dot( 3, 0 ), __Dot( 3, 1 ), __Dot( 3, 2 ), __Dot( 3, 3 ) );
}

V6_INLINE void Mat4x4_Transpose( Mat4x4* r )
{
	auto __Swap = [ r ]( uint raw, uint col)
	{
		float tmp;
		tmp = r->m_rows[raw].m_fValues[col];
		r->m_rows[raw].m_fValues[col] = r->m_rows[col].m_fValues[raw];
		r->m_rows[col].m_fValues[raw] = tmp;
	};

	__Swap( 0, 1 );
	__Swap( 0, 2 );
	__Swap( 0, 3 );
	__Swap( 1, 2 );
	__Swap( 1, 3 );
	__Swap( 2, 3 );
}

V6_INLINE Mat4x4 Mat4x4_Identity()
{
	Mat4x4 m;
	m.m_row0 = Vec4_Make( 1.0, 0.0, 0.0, 0.0 );
	m.m_row1 = Vec4_Make( 0.0, 1.0, 0.0, 0.0 );
	m.m_row2 = Vec4_Make( 0.0, 0.0, 1.0, 0.0 );
	m.m_row3 = Vec4_Make( 0.0, 0.0, 0.0, 1.0 );

	return m;
}

V6_INLINE Mat4x4 Mat4x4_RotationX( float a )
{
	float s, c;
	SinCos( a, &s, &c );
	Mat4x4 m;
	m.m_row0 = Vec4_Make(  1,  0,  0,  0 );
	m.m_row1 = Vec4_Make(  0,  c, -s,  0 );
	m.m_row2 = Vec4_Make(  0,  s,  c,  0 );
	m.m_row3 = Vec4_Make(  0,  0,  0,  1 );

	return m;
}

V6_INLINE Mat4x4 Mat4x4_RotationY( float a )
{
	float s, c;
	SinCos( a, &s, &c );
	Mat4x4 m;
	m.m_row0 = Vec4_Make(  c,  0,  s,  0 );
	m.m_row1 = Vec4_Make(  0,  1,  0,  0 );
	m.m_row2 = Vec4_Make( -s,  0,  c,  0 );
	m.m_row3 = Vec4_Make(  0,  0,  0,  1 );

	return m;
}

V6_INLINE Mat4x4 Mat4x4_RotationZ( float a )
{
	float s, c;
	SinCos( a, &s, &c );
	Mat4x4 m;
	m.m_row0 = Vec4_Make(  c, -s,  0,  0 );
	m.m_row1 = Vec4_Make(  s,  c,  0,  0 );
	m.m_row2 = Vec4_Make(  0,  0,  1,  0 );
	m.m_row3 = Vec4_Make(  0,  0,  0,  1 );

	return m;
}

V6_INLINE Mat4x4 Mat4x4_Projection( float n, float f, float fov, float aspectRatio )
{
	// [ w,	0, 0, 0 ]
	// [ 0,	h, 0, 0 ]
	// [ 0,	0, f/(f-n), 1 ]
	// [ 0,	0, -n*f/(f-n), 0 ]
	//
	// h = cot( fovY / 2.0 )
	// w = h / aspectRatio

	const float tanHalf = tan( fov * 0.5f );
	const float h = 1.0f / tanHalf;
	const float w = h / aspectRatio;
	const float fOverFMinusN = f / (f - n);

	Mat4x4 m;
	m.m_row0 = Vec4_Make(  w, 0, 0, 0 );
	m.m_row1 = Vec4_Make(  0, h, 0, 0 );
	m.m_row2 = Vec4_Make(  0, 0, fOverFMinusN, 1 );
	m.m_row3 = Vec4_Make(  0, 0, -n * fOverFMinusN,  0 );

	return m;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MAT4X4_H__