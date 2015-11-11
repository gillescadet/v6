/*V6*/

#pragma once

#ifndef __V6_CORE_VEC3I_H__
#define __V6_CORE_VEC3I_H__

#include <v6/core/math.h>

BEGIN_V6_CORE_NAMESPACE

struct Vec3i
{
public:
	int x;
	int y;
	int z;

public:
	Vec3i Abs() const
	{
		Vec3i v;
		v.x = abs( x );
		v.y = abs( y );
		v.z = abs( z );
		return v;
	}
		
	int LengthSq() const
	{
		return x * x + y * y + z * z;
	}
		
	int operator[](int nIndex) const { return ((int*)&x)[nIndex]; }
	int & operator[](int nIndex) { return ((int*)&x)[nIndex]; }

	Vec3i operator-() const
	{
		Vec3i v;
		v.x = -x;
		v.y = -y;
		v.z = -z;
		return v;
	}

	Vec3i& operator*=(Vec3i const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		z *= v2.z;
		return *this;
	}
	
	Vec3i& operator+=(Vec3i const & v2)
	{
		x += v2.x;
		y += v2.y;
		z += v2.z;
		return *this;
	}

	Vec3i& operator-=(Vec3i const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		z -= v2.z;
		return *this;
	}
};

V6_INLINE Vec3i Vec3i_Zero()
{
	Vec3i v;
	v.x = 0;
	v.y = 0;
	v.z = 0;

	return v;
}

V6_INLINE Vec3i Vec3i_Make( int x, int y, int z )
{
	Vec3i v;
	v.x = x;
	v.y = y;
	v.z = z;

	return v;
}

V6_INLINE int Dot( Vec3i const & v1, Vec3i const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

V6_INLINE Vec3i Cross( Vec3i const & v1, Vec3i const & v2 )
{
	Vec3i v;
	v.x = v1.y * v2.z - v1.z * v2.y;
	v.y = v1.z * v2.x - v1.x * v2.z;
	v.z = v1.x * v2.y - v1.y * v2.x;
	return v;
}

V6_INLINE Vec3i Min( Vec3i const & v1, Vec3i const & v2 )
{
	Vec3i v;
	v.x = Min(v1.x, v2.x);
	v.y = Min(v1.y, v2.y);
	v.z = Min(v1.z, v2.z);
	return v;
}

V6_INLINE Vec3i Max( Vec3i const & v1, Vec3i const & v2 )
{
	Vec3i v;
	v.x = Max(v1.x, v2.x);
	v.y = Max(v1.y, v2.y);
	v.z = Max(v1.z, v2.z);
	return v;
}

V6_INLINE Vec3i operator*(Vec3i const & v1, int f)
{
	Vec3i v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	return v;
}

V6_INLINE Vec3i operator*( int f, Vec3i const & v1 )
{
	Vec3i v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	return v;
}

V6_INLINE Vec3i operator+(Vec3i const & v1, int f)
{
	Vec3i v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	return v;
}

V6_INLINE Vec3i operator+(int f, Vec3i const & v1)
{
	Vec3i v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	return v;
}

V6_INLINE Vec3i operator-(Vec3i const & v1, int f)
{
	Vec3i v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	v.z = v1.z - f;
	return v;
}

V6_INLINE Vec3i operator-(int f, Vec3i const & v2)
{
	Vec3i v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	v.z = f - v2.z;
	return v;
}

V6_INLINE Vec3i operator*(Vec3i const & v1, Vec3i const & v2)
{
	Vec3i v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	v.z = v1.z * v2.z;
	return v;
}

V6_INLINE Vec3i operator+(Vec3i const & v1, Vec3i const & v2)
{
	Vec3i v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	v.z = v1.z + v2.z;
	return v;
}

V6_INLINE Vec3i operator-(Vec3i const & v1, Vec3i const & v2)
{
	Vec3i v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	v.z = v1.z - v2.z;
	return v;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_VEC3I_H__