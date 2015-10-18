/*V6*/

#pragma once

#ifndef __V6_CORE_MAT4X4_H__
#define __V6_CORE_MAT4X4_H__

#include <v6/core/math.h>
#include <v6/core/vec3.h>
#include <v6/core/vec4.h>

BEGIN_V6_CORE_NAMESPACE

// row-major notation and storage
// column-vector convention
// left to right concatenation of "from" local space transformations

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

	Vec3* GetXAxis() { return (Vec3*)&m_row0; }
	Vec3* GetYAxis() { return (Vec3*)&m_row1; }
	Vec3* GetZAxis() { return (Vec3*)&m_row2; }
};

V6_INLINE void Mat4x4_TransformDir( Vec3* r, const Mat4x4& m, const Vec3& v)
{
	r->x = m.m_rows[0].x * v.x + m.m_rows[0].y * v.y + m.m_rows[0].z * v.z;
	r->y = m.m_rows[1].x * v.x + m.m_rows[1].y * v.y + m.m_rows[1].z * v.z;
	r->z = m.m_rows[2].x * v.x + m.m_rows[2].y * v.y + m.m_rows[2].z * v.z;
}

V6_INLINE void Mat4x4_Mul( Mat4x4* r, const Mat4x4& a, const Mat4x4& b )
{
	const auto __Dot = [ r, a, b ]( uint raw, uint col ) -> float
	{
		return
			a.m_rows[raw].x * b.m_row0.m_fValues[col] +
			a.m_rows[raw].y * b.m_row1.m_fValues[col] +
			a.m_rows[raw].z * b.m_row2.m_fValues[col] +
			a.m_rows[raw].w * b.m_row3.m_fValues[col];
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

V6_INLINE void Mat4x4_PreScale( Mat4x4* r, float scale )
{
	r->m_row0.x *= scale;
	r->m_row0.y *= scale;
	r->m_row0.z *= scale;
	r->m_row1.x *= scale;
	r->m_row1.y *= scale;
	r->m_row1.z *= scale;
	r->m_row2.x *= scale;
	r->m_row2.y *= scale;
	r->m_row2.z *= scale;
}

V6_INLINE void Mat4x4_SetTranslation( Mat4x4* r, const Vec3& v )
{
	r->m_row0.w = v.x;
	r->m_row1.w = v.y;
	r->m_row2.w = v.z;
}

V6_INLINE void Mat4x4_AffineInverse( Mat4x4* r )
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
	__Swap( 1, 2 );

	Vec3 invT;
	Mat4x4_TransformDir( &invT, *r, Vec3_Make( -r->m_row0.w, -r->m_row1.w, -r->m_row2.w ) );;
	Mat4x4_SetTranslation( r, invT);
}

V6_INLINE void Mat4x4_Identity( Mat4x4* r )
{
	r->m_row0 = Vec4_Make( 1.0, 0.0, 0.0, 0.0 );
	r->m_row1 = Vec4_Make( 0.0, 1.0, 0.0, 0.0 );
	r->m_row2 = Vec4_Make( 0.0, 0.0, 1.0, 0.0 );
	r->m_row3 = Vec4_Make( 0.0, 0.0, 0.0, 1.0 );
}

V6_INLINE void Mat4x4_Scale( Mat4x4* r, float scale )
{
	r->m_row0 = Vec4_Make( scale, 0.0, 0.0, 0.0 );
	r->m_row1 = Vec4_Make( 0.0, scale, 0.0, 0.0 );
	r->m_row2 = Vec4_Make( 0.0, 0.0, scale, 0.0 );
	r->m_row3 = Vec4_Make( 0.0, 0.0, 0.0, 1.0 );
}

V6_INLINE Mat4x4 Mat4x4_Rotation( const Vec3& lookAt, const Vec3& up )
{
	const Vec3 right = Cross( lookAt, up );
	Mat4x4 m;
	m.m_row0 = Vec4_Make( right.x, up.x, -lookAt.x, 0 );
	m.m_row1 = Vec4_Make( right.y, up.y, -lookAt.y, 0 );
	m.m_row2 = Vec4_Make( right.z, up.z, -lookAt.z, 0 );
	m.m_row3 = Vec4_Make( 0,       0,     0,        1 );

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
	// [ 0,	0, f/(n-f), n*f/(n-f) ]
	// [ 0,	0, -1, 0 ]
	//
	// h = cot( fovY / 2.0 )
	// w = h / aspectRatio

	const float tanHalf = tan( fov * 0.5f );
	const float h = 1.0f / tanHalf;
	const float w = h / aspectRatio;
	const float q = f / (n - f);

	Mat4x4 m;
	m.m_row0 = Vec4_Make(  w, 0, 0, 0 );
	m.m_row1 = Vec4_Make(  0, h, 0, 0 );
#if 0
	m.m_row2 = Vec4_Make(  0, 0, q, n * q );
#else
	// Infinite far
	m.m_row2 = Vec4_Make(  0, 0, -1, -n );
#endif
	m.m_row3 = Vec4_Make(  0, 0, -1,  0 );

	return m;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MAT4X4_H__