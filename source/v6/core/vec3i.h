/*V6*/

#pragma once

#ifndef __V6_CORE_VEC3I_H__
#define __V6_CORE_VEC3I_H__

#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

template < typename INTEGER_TYPE >
struct Vec3_INTEGER
{
public:
	INTEGER_TYPE x;
	INTEGER_TYPE y;
	INTEGER_TYPE z;

public:
	Vec3_INTEGER< INTEGER_TYPE > Abs() const
	{
		Vec3_INTEGER< INTEGER_TYPE > v;
		v.x = abs( x );
		v.y = abs( y );
		v.z = abs( z );
		return v;
	}
		
	INTEGER_TYPE LengthSq() const
	{
		return x * x + y * y + z * z;
	}
		
	INTEGER_TYPE operator[](INTEGER_TYPE nIndex) const { return ((INTEGER_TYPE*)&x)[nIndex]; }
	INTEGER_TYPE & operator[](INTEGER_TYPE nIndex) { return ((INTEGER_TYPE*)&x)[nIndex]; }

	Vec3_INTEGER< INTEGER_TYPE > operator-() const
	{
		Vec3_INTEGER< INTEGER_TYPE > v;
		v.x = -x;
		v.y = -y;
		v.z = -z;
		return v;
	}

	Vec3_INTEGER< INTEGER_TYPE >& operator*=(Vec3_INTEGER< INTEGER_TYPE > const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		z *= v2.z;
		return *this;
	}
	
	Vec3_INTEGER< INTEGER_TYPE >& operator+=(Vec3_INTEGER< INTEGER_TYPE > const & v2)
	{
		x += v2.x;
		y += v2.y;
		z += v2.z;
		return *this;
	}

	Vec3_INTEGER< INTEGER_TYPE >& operator-=(Vec3_INTEGER< INTEGER_TYPE > const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		z -= v2.z;
		return *this;
	}
};

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > Vec3_INTEGER_Zero()
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = 0;
	v.y = 0;
	v.z = 0;

	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > Vec3_INTEGER_Make( INTEGER_TYPE x, INTEGER_TYPE y, INTEGER_TYPE z )
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = x;
	v.y = y;
	v.z = z;

	return v;
}

template < typename INTEGER_TYPE >
INTEGER_TYPE Dot( Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > Cross( Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.y * v2.z - v1.z * v2.y;
	v.y = v1.z * v2.x - v1.x * v2.z;
	v.z = v1.x * v2.y - v1.y * v2.x;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > Min( Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = Min(v1.x, v2.x);
	v.y = Min(v1.y, v2.y);
	v.z = Min(v1.z, v2.z);
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > Max( Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = Max(v1.x, v2.x);
	v.y = Max(v1.y, v2.y);
	v.z = Max(v1.z, v2.z);
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator*(Vec3_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator*( INTEGER_TYPE f, Vec3_INTEGER< INTEGER_TYPE > const & v1 )
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator+(Vec3_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator+(INTEGER_TYPE f, Vec3_INTEGER< INTEGER_TYPE > const & v1)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator-(Vec3_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	v.z = v1.z - f;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator-(INTEGER_TYPE f, Vec3_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	v.z = f - v2.z;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator*(Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	v.z = v1.z * v2.z;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator+(Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	v.z = v1.z + v2.z;
	return v;
}

template < typename INTEGER_TYPE >
Vec3_INTEGER< INTEGER_TYPE > operator-(Vec3_INTEGER< INTEGER_TYPE > const & v1, Vec3_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec3_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	v.z = v1.z - v2.z;
	return v;
}

typedef Vec3_INTEGER< int >			Vec3i;
typedef Vec3_INTEGER< u32 >	Vec3u;

V6_INLINE Vec3i Vec3i_Zero()											{ return Vec3_INTEGER_Zero< int >(); }
V6_INLINE Vec3i Vec3i_Make( int x, int y, int z )						{ return Vec3_INTEGER_Make< int >( x, y, z); }

V6_INLINE Vec3u Vec3u_Zero()											{ return Vec3_INTEGER_Zero< u32 >(); }
V6_INLINE Vec3u Vec3u_Make( u32 x, u32 y, u32 z )		{ return Vec3_INTEGER_Make< u32 >( x, y, z); }

END_V6_NAMESPACE

#endif // __V6_CORE_VEC3I_H__