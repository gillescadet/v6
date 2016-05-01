/*V6*/

#pragma once

#ifndef __V6_CORE_VEC4_H__
#define __V6_CORE_VEC4_H__

#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

struct Vec4
{
public:
	union
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
		float m_fValues[4];
	};	

public:
	float Length() const
	{
		return Sqrt(LengthSq());
	}

	float LengthSq() const
	{
		return x * x + y * y + z * z + w * w;
	}

	Vec4 Normalized() const
	{
		float const fL = Length();
		if (fL > FLT_EPSILON)
		{
			float const fInvL = 1.0f / fL;
			Vec4 v;
			v.x = x * fInvL;
			v.y = y * fInvL;
			v.z = z * fInvL;
			v.w = w * fInvL;
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
			z *= fInvL;
			w *= fInvL;
		}
	}

	float operator[](int nIndex) const { return ((float*)&x)[nIndex]; }
	float & operator[](int nIndex) { return ((float*)&x)[nIndex]; }

	Vec4 operator-() const
	{
		Vec4 v;
		v.x = -x;
		v.y = -y;
		v.z = -z;
		v.w = -w;
		return v;
	}

	Vec4 operator*=(Vec4 const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		z *= v2.z;
		w *= v2.w;
	}
	
	Vec4 operator+=(Vec4 const & v2)
	{
		x += v2.x;
		y += v2.y;
		z += v2.z;
		w += v2.w;
	}

	Vec4 operator-=(Vec4 const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		z -= v2.z;
		w -= v2.w;
	}
};

V6_INLINE Vec4 Vec4_Make( float x, float y, float z, float w )
{
	Vec4 v;
	v.x = x;
	v.y = y;
	v.z = z;
	v.w = w;

	return v;
}

V6_INLINE Vec4 Vec4_Make( const Vec3* xyz, float w )
{
	Vec4 v;
	v.x = xyz->x;
	v.y = xyz->y;	
	v.z = xyz->z;
	v.w = w;

	return v;
}

V6_INLINE float Dot( Vec4 const & v1, Vec4 const & v2 )
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

V6_INLINE Vec4 Min( Vec4 const & v1, Vec4 const & v2 )
{
	Vec4 v;
	v.x = Min( v1.x, v2.x );
	v.y = Min( v1.y, v2.y );
	v.z = Min( v1.z, v2.z );
	v.w = Min( v1.w, v2.w );
	return v;
}

V6_INLINE Vec4 Max( Vec4 const & v1, Vec4 const & v2)
{
	Vec4 v;
	v.x = Max( v1.x, v2.x );
	v.y = Max( v1.y, v2.y );
	v.z = Max( v1.z, v2.z );
	v.w = Max( v1.w, v2.w );
	return v;					   
}

V6_INLINE Vec4 operator*( Vec4 const & v1, float f)
{
	Vec4 v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	v.w = v1.w * f;
	return v;
}

V6_INLINE Vec4 operator+( Vec4 const & v1, float f )
{
	Vec4 v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	v.w = v1.w + f;
	return v;
}

V6_INLINE Vec4 operator-( Vec4 const & v1, float f )
{
	Vec4 v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	v.z = v1.z - f;
	v.w = v1.w - f;
	return v;
}

V6_INLINE Vec4 operator-( float f, Vec4 const & v2 )
{
	Vec4 v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	v.z = f - v2.z;
	v.w = f - v2.w;
	return v;
}

V6_INLINE Vec4 operator*( Vec4 const & v1, Vec4 const & v2 )
{
	Vec4 v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	v.z = v1.z * v2.z;
	v.w = v1.w * v2.w;
	return v;
}

V6_INLINE Vec4 operator+( Vec4 const & v1, Vec4 const & v2 )
{
	Vec4 v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	v.z = v1.z + v2.z;
	v.w = v1.w + v2.w;
	return v;
}

V6_INLINE Vec4 operator-( Vec4 const & v1, Vec4 const & v2 )
{
	Vec4 v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	v.z = v1.z - v2.z;
	v.w = v1.w - v2.w;
	return v;
}

END_V6_NAMESPACE

#endif // __V6_CORE_VEC4_H__