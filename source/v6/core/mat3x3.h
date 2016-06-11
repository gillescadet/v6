/*V6*/

#pragma once

#ifndef __V6_CORE_MAT3X3_H__
#define __V6_CORE_MAT3X3_H__

#include <v6/core/math.h>
#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

// row-major notation and storage
// column-vector convention
// left to right concatenation of "from" local space transformations

struct Mat3x3
{
public:
	union
	{
		struct
		{
			Vec3 m_row0;
			Vec3 m_row1;
			Vec3 m_row2;
		};
		Vec3 m_rows[3];
	};	

	void GetXAxis( Vec3* v ) const { Vec3_Make( v, m_row0.x, m_row1.x, m_row2.x ); }
	void GetYAxis( Vec3* v ) const { Vec3_Make( v, m_row0.y, m_row1.y, m_row2.y ); }
	void GetZAxis( Vec3* v ) const { Vec3_Make( v, m_row0.z, m_row1.z, m_row2.z ); }
};

V6_INLINE void Mat3x3_TransformDir( Vec3* r, const Mat3x3& m, const Vec3& v)
{
	r->x = m.m_rows[0].x * v.x + m.m_rows[0].y * v.y + m.m_rows[0].z * v.z;
	r->y = m.m_rows[1].x * v.x + m.m_rows[1].y * v.y + m.m_rows[1].z * v.z;
	r->z = m.m_rows[2].x * v.x + m.m_rows[2].y * v.y + m.m_rows[2].z * v.z;
}

V6_INLINE void Mat3x3_InverseTransformDir( Vec3* r, const Mat3x3& m, const Vec3& v)
{
	r->x = m.m_rows[0].x * v.x + m.m_rows[1].x * v.y + m.m_rows[2].x * v.z;
	r->y = m.m_rows[0].y * v.x + m.m_rows[1].y * v.y + m.m_rows[2].y * v.z;
	r->z = m.m_rows[0].z * v.x + m.m_rows[1].z * v.y + m.m_rows[2].z * v.z;
}

V6_INLINE void Mat3x3_Mul( Mat3x3* r, const Mat3x3& a, const Mat3x3& b )
{
	const auto __Dot = [ r, a, b ]( u32 raw, u32 col ) -> float
	{
		return
			a.m_rows[raw].x * b.m_row0.values[col] +
			a.m_rows[raw].y * b.m_row1.values[col] +
			a.m_rows[raw].z * b.m_row2.values[col];
	};

	r->m_row0 = Vec3_Make( __Dot( 0, 0 ), __Dot( 0, 1 ), __Dot( 0, 2 ) );
	r->m_row1 = Vec3_Make( __Dot( 1, 0 ), __Dot( 1, 1 ), __Dot( 1, 2 ) );
	r->m_row2 = Vec3_Make( __Dot( 2, 0 ), __Dot( 2, 1 ), __Dot( 2, 2 ) );
}

V6_INLINE void Mat3x3_Transpose( Mat3x3* r )
{
	auto __Swap = [ r ]( u32 raw, u32 col)
	{
		float tmp;
		tmp = r->m_rows[raw].values[col];
		r->m_rows[raw].values[col] = r->m_rows[col].values[raw];
		r->m_rows[col].values[raw] = tmp;
	};

	__Swap( 0, 1 );
	__Swap( 0, 2 );
	__Swap( 1, 2 );
}

V6_INLINE void Mat3x3_PreScale( Mat3x3* r, float scale )
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

V6_INLINE void Mat3x3_Clear( Mat3x3* r )
{
	r->m_row0 = Vec3_Zero();
	r->m_row1 = Vec3_Zero();
	r->m_row2 = Vec3_Zero();
}

template < bool PRECENTRED >
V6_INLINE void Mat3x3_Covariance_Guts( Mat3x3* r, Vec3* center, const Vec3* points, u32 pointCount )
{
	V6_ASSERT( pointCount > 0 );
	
	const float invPointCount = 1.0f / pointCount;

	if ( !PRECENTRED )
	{
		*center = Vec3_Zero();
		for ( u32 pointID = 0; pointID < pointCount; ++pointID )
			*center += points[pointID];
		*center *= invPointCount;
	}

	float sumXX = 0.0f;
	float sumXY = 0.0f;
	float sumXZ = 0.0f;

	float sumYY = 0.0f;
	float sumYZ = 0.0f;

	float sumZZ = 0.0f;

	for ( u32 pointID = 0; pointID < pointCount; ++pointID )
	{
		Vec3 p = points[pointID];
		if ( !PRECENTRED )
			p -= *center;
		
		sumXX += p.x * p.x;
		sumXY += p.x * p.y;
		sumXZ += p.x * p.z;

		sumYY += p.y * p.y;
		sumYZ += p.y * p.z;

		sumZZ += p.z * p.z;
	}

	sumXX *= invPointCount;
	sumXY *= invPointCount;
	sumXZ *= invPointCount;

	sumYY *= invPointCount;
	sumYZ *= invPointCount;

	sumZZ *= invPointCount;

	r->m_row0 = Vec3_Make( sumXX, sumXY, sumXZ );
	r->m_row1 = Vec3_Make( sumXY, sumYY, sumYZ );
	r->m_row2 = Vec3_Make( sumXZ, sumYZ, sumZZ );
}

V6_INLINE void Mat3x3_Covariance( Mat3x3* r, Vec3* center, const Vec3* points, u32 pointCount )
{
	Mat3x3_Covariance_Guts< true >( r, center, points, pointCount );
}

V6_INLINE void Mat3x3_CovariancePrecentred( Mat3x3* r, const Vec3* points, u32 pointCount )
{
	Mat3x3_Covariance_Guts< true >( r, nullptr, points, pointCount );
}

V6_INLINE void Mat3x3_Identity( Mat3x3* r )
{
	r->m_row0 = Vec3_Make( 1.0, 0.0, 0.0 );
	r->m_row1 = Vec3_Make( 0.0, 1.0, 0.0 );
	r->m_row2 = Vec3_Make( 0.0, 0.0, 1.0 );
}

V6_INLINE void Mat3x3_Scale( Mat3x3* r, float scale )
{
	r->m_row0 = Vec3_Make( scale, 0.0, 0.0 );
	r->m_row1 = Vec3_Make( 0.0, scale, 0.0 );
	r->m_row2 = Vec3_Make( 0.0, 0.0, scale );
}

V6_INLINE Mat3x3 Mat3x3_Rotation( const Vec3& lookAt, const Vec3& up )
{
	const Vec3 right = Cross( lookAt, up );
	Mat3x3 m;
	m.m_row0 = Vec3_Make( right.x, up.x, -lookAt.x );
	m.m_row1 = Vec3_Make( right.y, up.y, -lookAt.y );
	m.m_row2 = Vec3_Make( right.z, up.z, -lookAt.z );

	return m;
}

V6_INLINE Mat3x3 Mat3x3_RotationX( float a )
{
	float s, c;
	SinCos( a, &s, &c );
	Mat3x3 m;
	m.m_row0 = Vec3_Make(  1,  0,  0 );
	m.m_row1 = Vec3_Make(  0,  c, -s );
	m.m_row2 = Vec3_Make(  0,  s,  c );

	return m;
}

V6_INLINE Mat3x3 Mat3x3_RotationY( float a )
{
	float s, c;
	SinCos( a, &s, &c );
	Mat3x3 m;
	m.m_row0 = Vec3_Make(  c,  0,  s );
	m.m_row1 = Vec3_Make(  0,  1,  0 );
	m.m_row2 = Vec3_Make( -s,  0,  c );

	return m;
}

V6_INLINE Mat3x3 Mat3x3_RotationZ( float a )
{
	float s, c;
	SinCos( a, &s, &c );
	Mat3x3 m;
	m.m_row0 = Vec3_Make(  c, -s,  0 );
	m.m_row1 = Vec3_Make(  s,  c,  0 );
	m.m_row2 = Vec3_Make(  0,  0,  1 );

	return m;
}

END_V6_NAMESPACE

#endif // __V6_CORE_MAT3X3_H__