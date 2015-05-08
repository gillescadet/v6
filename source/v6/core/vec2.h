/*V6*/

#pragma once

#ifndef __V6_CORE_VEC2_H__
#define __V6_CORE_VEC2_H__

#include <v6/core/math.h>

BEGIN_V6_CORE_NAMESPACE

struct Vec2
{
public:
	union
	{
		struct
		{
			float x;
			float y;
		};
		float m_fValues[2];
	};	

public:
	float Length() const
	{
		return Sqrt(LengthSq());
	}

	float LengthSq() const
	{
		return x * x + y * y;
	}

	Vec2 Normalized() const
	{
		float const fL = Length();
		if (fL > FLT_EPSILON)
		{
			float const fInvL = 1.0f / fL;
			Vec2 v;
			v.x = x * fInvL;
			v.y = y * fInvL;
			return v;
		}
		return *this;
	}

	void Normalize()
	{
		float const fL = Length();
		if (fL > FLT_EPSILON)
		{
			float const fInvL = 1.0f / fL;
			x *= fInvL;
			y *= fInvL;
		}
	}

	float operator[](int nIndex) const { return ((float*)&x)[nIndex]; }
	float & operator[](int nIndex) { return ((float*)&x)[nIndex]; }

	Vec2 operator-() const
	{
		Vec2 v;
		v.x = -x;
		v.y = -y;
		return v;
	}

	Vec2 operator*=(Vec2 const & v2)
	{
		x *= v2.x;
		y *= v2.y;
	}
	
	Vec2 operator+=(Vec2 const & v2)
	{
		x += v2.x;
		y += v2.y;
	}

	Vec2 operator-=(Vec2 const & v2)
	{
		x -= v2.x;
		y -= v2.y;
	}
};

V6_INLINE Vec2 Vec2_Make( float x, float y )
{
	Vec2 v;
	v.x = x;
	v.y = y;

	return v;
}

V6_INLINE float Dot( Vec2 const & v1, Vec2 const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y;
}

V6_INLINE Vec2 Min( Vec2 const & v1, Vec2 const & v2 )
{
	Vec2 v;
	v.x = Min( v1.x, v2.x );
	v.y = Min( v1.y, v2.y );
	return v;
}

V6_INLINE Vec2 Max( Vec2 const & v1, Vec2 const & v2)
{
	Vec2 v;
	v.x = Max( v1.x, v2.x );
	v.y = Max( v1.y, v2.y );
return v;					   
}

V6_INLINE Vec2 operator*( Vec2 const & v1, float f)
{
	Vec2 v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	return v;
}

V6_INLINE Vec2 operator+( Vec2 const & v1, float f )
{
	Vec2 v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	return v;
}

V6_INLINE Vec2 operator-( Vec2 const & v1, float f )
{
	Vec2 v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	return v;
}

V6_INLINE Vec2 operator-( float f, Vec2 const & v2 )
{
	Vec2 v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	return v;
}

V6_INLINE Vec2 operator*( Vec2 const & v1, Vec2 const & v2 )
{
	Vec2 v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	return v;
}

V6_INLINE Vec2 operator+( Vec2 const & v1, Vec2 const & v2 )
{
	Vec2 v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	return v;
}

V6_INLINE Vec2 operator-( Vec2 const & v1, Vec2 const & v2 )
{
	Vec2 v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	return v;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_VEC2_H__