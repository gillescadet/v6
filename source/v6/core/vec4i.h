/*V6*/

#pragma once

#ifndef __V6_CORE_VEC4I_H__
#define __V6_CORE_VEC4I_H__

#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

template < typename INTEGER_TYPE > 
struct Vec4_INTEGER
{
public:
	INTEGER_TYPE x;
	INTEGER_TYPE y;
	INTEGER_TYPE z;
	INTEGER_TYPE w;

public:
	Vec4_INTEGER< INTEGER_TYPE > Abs() const
	{
		Vec4_INTEGER< INTEGER_TYPE > v;
		v.x = abs( x );
		v.y = abs( y );
		v.z = abs( z );
		v.w = abs( w );
		return v;
	}

	INTEGER_TYPE LengthSq() const
	{
		return x * x + y * y + z * z + w * w;
	}

	INTEGER_TYPE operator[](INTEGER_TYPE nIndex) const { return ((INTEGER_TYPE*)&x)[nIndex]; }
	INTEGER_TYPE & operator[](INTEGER_TYPE nIndex) { return ((INTEGER_TYPE*)&x)[nIndex]; }

	Vec4_INTEGER< INTEGER_TYPE > operator-() const
	{
		Vec4_INTEGER< INTEGER_TYPE > v;
		v.x = -x;
		v.y = -y;
		v.z = -z;
		v.w = -w;
		return v;
	}

	Vec4_INTEGER< INTEGER_TYPE >& operator*=(Vec4_INTEGER< INTEGER_TYPE > const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		z *= v2.z;
		w *= v2.w;
		return *this;
	}

	Vec4_INTEGER< INTEGER_TYPE >& operator+=(Vec4_INTEGER< INTEGER_TYPE > const & v2)
	{
		x += v2.x;
		y += v2.y;
		z += v2.z;
		w += v2.w;
		return *this;
	}

	Vec4_INTEGER< INTEGER_TYPE >& operator-=(Vec4_INTEGER< INTEGER_TYPE > const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		z -= v2.z;
		w -= v2.w;
		return *this;
	}
};

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > Vec4_INTEGER_Zero()
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = 0;
	v.y = 0;
	v.z = 0;
	v.w = 0;

	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > Vec4_INTEGER_Make( INTEGER_TYPE x, INTEGER_TYPE y, INTEGER_TYPE z, INTEGER_TYPE w )
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = x;
	v.y = y;
	v.z = z;
	v.w = w;

	return v;
}

template < typename INTEGER_TYPE >
INTEGER_TYPE Dot( Vec4_INTEGER< INTEGER_TYPE > const & v1, Vec4_INTEGER< INTEGER_TYPE > const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > Min( Vec4_INTEGER< INTEGER_TYPE > const & v1, Vec4_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = Min(v1.x, v2.x);
	v.y = Min(v1.y, v2.y);
	v.z = Min(v1.z, v2.z);
	v.w = Min(v1.w, v2.w);
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > Max( Vec4_INTEGER< INTEGER_TYPE > const & v1, Vec4_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = Max(v1.x, v2.x);
	v.y = Max(v1.y, v2.y);
	v.z = Max(v1.z, v2.z);
	v.w = Max(v1.w, v2.w);
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator*(Vec4_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	v.w = v1.w * f;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator*( INTEGER_TYPE f, Vec4_INTEGER< INTEGER_TYPE > const & v1 )
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	v.w = v1.w * f;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator+(Vec4_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	v.w = v1.w + f;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator+(INTEGER_TYPE f, Vec4_INTEGER< INTEGER_TYPE > const & v1)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	v.w = v1.w + f;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator-(Vec4_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	v.z = v1.z - f;
	v.w = v1.w - f;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator-(INTEGER_TYPE f, Vec4_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	v.z = f - v2.z;
	v.w = f - v2.w;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator*(Vec4_INTEGER< INTEGER_TYPE > const & v1, Vec4_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	v.z = v1.z * v2.z;
	v.w = v1.w * v2.w;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator+(Vec4_INTEGER< INTEGER_TYPE > const & v1, Vec4_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	v.z = v1.z + v2.z;
	v.w = v1.w + v2.w;
	return v;
}

template < typename INTEGER_TYPE >
Vec4_INTEGER< INTEGER_TYPE > operator-(Vec4_INTEGER< INTEGER_TYPE > const & v1, Vec4_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec4_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	v.z = v1.z - v2.z;
	v.w = v1.w - v2.w;
	return v;
}

typedef Vec4_INTEGER< int >			Vec4i;
typedef Vec4_INTEGER< u32 >			Vec4u;

V6_INLINE Vec4i Vec4i_Zero()														{ return Vec4_INTEGER_Zero< int >(); }
V6_INLINE Vec4i Vec4i_Make( int x, int y, int z, int w )							{ return Vec4_INTEGER_Make< int >( x, y, z, w ); }

V6_INLINE Vec4u Vec4u_Zero()														{ return Vec4_INTEGER_Zero< u32 >(); }
V6_INLINE Vec4u Vec4u_Make( u32 x, u32 y, u32 z, u32 w )	{ return Vec4_INTEGER_Make< u32 >( x, y, z, w ); }

END_V6_NAMESPACE

#endif // __V6_CORE_VEC4I_H__