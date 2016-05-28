/*V6*/

#pragma once

#ifndef __V6_CORE_VEC3_H__
#define __V6_CORE_VEC3_H__

#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

struct Vec3
{
public:
	union
	{
		struct
		{
			float x;
			float y;
			float z;
		};
		float values[3];
	};

public:
	Vec3 Abs() const
	{
		Vec3 v;
		v.x = fabsf( x );
		v.y = fabsf( y );
		v.z = fabsf( z );
		return v;
	}

	float Length() const
	{
		return Sqrt(LengthSq());
	}

	float LengthSq() const
	{
		return x * x + y * y + z * z;
	}

	Vec3 Normalized() const
	{
		float const fL = Length();
		if (fL > FLT_EPSILON)
		{
			float const fInvL = 1.0f / fL;
			Vec3 v;
			v.x = x * fInvL;
			v.y = y * fInvL;
			v.z = z * fInvL;
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
		}
	}

	Vec3 Rcp() const
	{
		Vec3 v;
		v.x = 1.0f / (fabs( x ) > FLT_EPSILON ? x : FLT_EPSILON);
		v.y = 1.0f / (fabs( y ) > FLT_EPSILON ? y : FLT_EPSILON);
		v.z = 1.0f / (fabs( z ) > FLT_EPSILON ? z : FLT_EPSILON);

		return v;
	}

	float operator[](int nIndex) const { return ((float*)&x)[nIndex]; }
	float & operator[](int nIndex) { return ((float*)&x)[nIndex]; }

	bool operator==( const Vec3& o ) const
	{
		return x == o.x && y == o.y && z == o.z;
	}

	bool operator!=( const Vec3& o ) const
	{
		return x != o.x || y != o.y || z != o.z;
	}

	Vec3 operator-() const
	{
		Vec3 v;
		v.x = -x;
		v.y = -y;
		v.z = -z;
		return v;
	}

	Vec3& operator*=(Vec3 const & v2)
	{
		x *= v2.x;
		y *= v2.y;
		z *= v2.z;
		return *this;
	}

	Vec3& operator*=( float s )
	{
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	
	Vec3& operator+=(Vec3 const & v2)
	{
		x += v2.x;
		y += v2.y;
		z += v2.z;
		return *this;
	}

	Vec3& operator+=( float s )
	{
		x += s;
		y += s;
		z += s;
		return *this;
	}

	Vec3& operator-=(Vec3 const & v2)
	{
		x -= v2.x;
		y -= v2.y;
		z -= v2.z;
		return *this;
	}

	Vec3& operator-=( float s )
	{
		x -= s;
		y -= s;
		z -= s;
		return *this;
	}
};

V6_INLINE Vec3 Vec3_Zero()
{
	Vec3 v;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = 0.0f;

	return v;
}

V6_INLINE Vec3 Vec3_Make( float x, float y, float z )
{
	Vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;

	return v;
}

V6_INLINE void Vec3_Make( Vec3* v, float x, float y, float z )
{
	v->x = x;
	v->y = y;
	v->z = z;
}

V6_INLINE Vec3 Vec3_Rand()
{
	static const float s_invRandSize = 2.0f / RAND_MAX;
	
	Vec3 v;
	v.x = -1.0f + rand() * s_invRandSize;
	v.y = -1.0f + rand() * s_invRandSize;
	v.z = -1.0f + rand() * s_invRandSize;

	return v;
}

V6_INLINE float Dot(Vec3 const & v1, Vec3 const & v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

V6_INLINE Vec3 Cross(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.x = v1.y * v2.z - v1.z * v2.y;
	v.y = v1.z * v2.x - v1.x * v2.z;
	v.z = v1.x * v2.y - v1.y * v2.x;
	return v;
}

V6_INLINE Vec3 Min(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.x = Min(v1.x, v2.x);
	v.y = Min(v1.y, v2.y);
	v.z = Min(v1.z, v2.z);
	return v;
}

V6_INLINE Vec3 Max(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.x = Max(v1.x, v2.x);
	v.y = Max(v1.y, v2.y);
	v.z = Max(v1.z, v2.z);
	return v;
}

V6_INLINE Vec3 operator*(Vec3 const & v1, float f)
{
	Vec3 v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	return v;
}

V6_INLINE Vec3 operator*( float f, Vec3 const & v1 )
{
	Vec3 v;
	v.x = v1.x * f;
	v.y = v1.y * f;
	v.z = v1.z * f;
	return v;
}

V6_INLINE Vec3 operator+(Vec3 const & v1, float f)
{
	Vec3 v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	return v;
}

V6_INLINE Vec3 operator+(float f, Vec3 const & v1)
{
	Vec3 v;
	v.x = v1.x + f;
	v.y = v1.y + f;
	v.z = v1.z + f;
	return v;
}

V6_INLINE Vec3 operator-(Vec3 const & v1, float f)
{
	Vec3 v;
	v.x = v1.x - f;
	v.y = v1.y - f;
	v.z = v1.z - f;
	return v;
}

V6_INLINE Vec3 operator-(float f, Vec3 const & v2)
{
	Vec3 v;
	v.x = f - v2.x;
	v.y = f - v2.y;
	v.z = f - v2.z;
	return v;
}

V6_INLINE Vec3 operator*(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.x = v1.x * v2.x;
	v.y = v1.y * v2.y;
	v.z = v1.z * v2.z;
	return v;
}

V6_INLINE Vec3 operator+(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.x = v1.x + v2.x;
	v.y = v1.y + v2.y;
	v.z = v1.z + v2.z;
	return v;
}

V6_INLINE Vec3 operator-(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.x = v1.x - v2.x;
	v.y = v1.y - v2.y;
	v.z = v1.z - v2.z;
	return v;
}

END_V6_NAMESPACE

#endif // __V6_CORE_VEC3_H__