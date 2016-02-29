/*V6*/

#pragma once

#ifndef __V6_CORE_VEC2I_H__
#define __V6_CORE_VEC2I_H__

#include <v6/core/math.h>

BEGIN_V6_CORE_NAMESPACE

struct Vec2i
{
public:
	int x;
	int y;

public:
	Vec2i Abs() const
	{
		Vec2i v;
		v.x = abs( x );
		v.y = abs( y );
		return v;
	}
		
	int LengthSq() const
	{
		return x * x + y * y;
	}
		
	int operator[](int nIndex) const { return ((int*)&x)[nIndex]; }
	int & operator[](int nIndex) { return ((int*)&x)[nIndex]; }

	Vec2i operator-() const
	{
		Vec2i v;
		v.x = -x;
		v.y = -y;
		return v;
	}

	Vec2i& operator*=(Vec2i const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		return *this;
	}
	
	Vec2i& operator+=(Vec2i const & v2)
	{
		x += v2.x;
		y += v2.y;
		return *this;
	}

	Vec2i& operator-=(Vec2i const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		return *this;
	}
};

V6_INLINE Vec2i Vec2i_Zero()
{
	Vec2i v;
	v.x = 0;
	v.y = 0;

	return v;
}

V6_INLINE Vec2i Vec2i_Make( int x, int y )
{
	Vec2i v;
	v.x = x;
	v.y = y;

	return v;
}

V6_INLINE int Dot( Vec2i const & v1, Vec2i const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y;
}

V6_INLINE Vec2i Min( Vec2i const & v1, Vec2i const & v2 )
{
	Vec2i v;
	v.x = Min(v1.x, v2.x);
	v.y = Min(v1.y, v2.y);
	return v;
}

V6_INLINE Vec2i Max( Vec2i const & v1, Vec2i const & v2 )
{
	Vec2i v;
	v.x = Max(v1.x, v2.x);
	v.y = Max(v1.y, v2.y);
	return v;
}

V6_INLINE Vec2i operator*(Vec2i const & v1, int f)
{
	Vec2i v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	return v;
}

V6_INLINE Vec2i operator*( int f, Vec2i const & v1 )
{
	Vec2i v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	return v;
}

V6_INLINE Vec2i operator+(Vec2i const & v1, int f)
{
	Vec2i v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	return v;
}

V6_INLINE Vec2i operator+(int f, Vec2i const & v1)
{
	Vec2i v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	return v;
}

V6_INLINE Vec2i operator-(Vec2i const & v1, int f)
{
	Vec2i v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	return v;
}

V6_INLINE Vec2i operator-(int f, Vec2i const & v2)
{
	Vec2i v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	return v;
}

V6_INLINE Vec2i operator*(Vec2i const & v1, Vec2i const & v2)
{
	Vec2i v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	return v;
}

V6_INLINE Vec2i operator+(Vec2i const & v1, Vec2i const & v2)
{
	Vec2i v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	return v;
}

V6_INLINE Vec2i operator-(Vec2i const & v1, Vec2i const & v2)
{
	Vec2i v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	return v;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_VEC2I_H__