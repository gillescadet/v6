/*V6*/

#pragma once

#ifndef __V6_CORE_Vec2i_H__
#define __V6_CORE_Vec2i_H__

#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

template< typename INTEGER_TYPE >
struct Vec2_INTEGER
{
public:
	INTEGER_TYPE x;
	INTEGER_TYPE y;

public:
	Vec2_INTEGER< INTEGER_TYPE > Abs() const
	{
		Vec2_INTEGER< INTEGER_TYPE > v;
		v.x = abs( x );
		v.y = abs( y );
		return v;
	}
		
	INTEGER_TYPE LengthSq() const
	{
		return x * x + y * y;
	}
		
	INTEGER_TYPE operator[](INTEGER_TYPE nIndex) const { return ((INTEGER_TYPE*)&x)[nIndex]; }
	INTEGER_TYPE & operator[](INTEGER_TYPE nIndex) { return ((INTEGER_TYPE*)&x)[nIndex]; }

	Vec2_INTEGER< INTEGER_TYPE > operator-() const
	{
		Vec2_INTEGER< INTEGER_TYPE > v;
		v.x = -x;
		v.y = -y;
		return v;
	}

	Vec2_INTEGER< INTEGER_TYPE >& operator*=(Vec2_INTEGER< INTEGER_TYPE > const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		return *this;
	}
	
	Vec2_INTEGER< INTEGER_TYPE >& operator+=(Vec2_INTEGER< INTEGER_TYPE > const & v2)
	{
		x += v2.x;
		y += v2.y;
		return *this;
	}

	Vec2_INTEGER< INTEGER_TYPE >& operator-=(Vec2_INTEGER< INTEGER_TYPE > const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		return *this;
	}
};

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > Vec2_INTEGER_Zero()
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = 0;
	v.y = 0;

	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > Vec2_INTEGER_Make( INTEGER_TYPE x, INTEGER_TYPE y )
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = x;
	v.y = y;

	return v;
}

template < typename INTEGER_TYPE >
INTEGER_TYPE Dot( Vec2_INTEGER< INTEGER_TYPE > const & v1, Vec2_INTEGER< INTEGER_TYPE > const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > Min( Vec2_INTEGER< INTEGER_TYPE > const & v1, Vec2_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = Min(v1.x, v2.x);
	v.y = Min(v1.y, v2.y);
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > Max( Vec2_INTEGER< INTEGER_TYPE > const & v1, Vec2_INTEGER< INTEGER_TYPE > const & v2 )
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = Max(v1.x, v2.x);
	v.y = Max(v1.y, v2.y);
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator*( Vec2_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f )
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator*( INTEGER_TYPE f, Vec2_INTEGER< INTEGER_TYPE > const & v1 )
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator+( Vec2_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f )
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator+(INTEGER_TYPE f, Vec2_INTEGER< INTEGER_TYPE > const & v1)
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator-(Vec2_INTEGER< INTEGER_TYPE > const & v1, INTEGER_TYPE f)
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator-(INTEGER_TYPE f, Vec2_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator*(Vec2_INTEGER< INTEGER_TYPE > const & v1, Vec2_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator+(Vec2_INTEGER< INTEGER_TYPE > const & v1, Vec2_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	return v;
}

template < typename INTEGER_TYPE >
Vec2_INTEGER< INTEGER_TYPE > operator-(Vec2_INTEGER< INTEGER_TYPE > const & v1, Vec2_INTEGER< INTEGER_TYPE > const & v2)
{
	Vec2_INTEGER< INTEGER_TYPE > v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	return v;
}

typedef Vec2_INTEGER< int >			Vec2i;
typedef Vec2_INTEGER< u32 >	Vec2u;

V6_INLINE Vec2i Vec2i_Zero()								{ return Vec2_INTEGER_Zero< int >(); }
V6_INLINE Vec2i Vec2i_Make( int x, int y )					{ return Vec2_INTEGER_Make< int >( x, y ); }

V6_INLINE Vec2u Vec2u_Zero()								{ return Vec2_INTEGER_Zero< u32 >(); }
V6_INLINE Vec2u Vec2u_Make( u32 x, u32 y )		{ return Vec2_INTEGER_Make< u32 >( x, y ); }

END_V6_NAMESPACE

#endif // __V6_CORE_Vec2i_H__