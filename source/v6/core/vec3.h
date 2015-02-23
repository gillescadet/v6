/*V6*/

#pragma once

#ifndef __V6_CORE_VEC3_H__
#define __V6_CORE_VEC3_H__

#include <v6/core/math.h>

BEGIN_V6_CORE_NAMESPACE

struct Vec3
{
public:
	union
	{
		struct
		{
			float m_fX;
			float m_fY;
			float m_fZ;
		};
		float m_fValues[3];
	};	

public:
	float Length() const
	{
		return Sqrt(LengthSq());
	}

	float LengthSq() const
	{
		return m_fX * m_fX + m_fY * m_fY + m_fZ * m_fZ;
	}

	Vec3 Normalized() const
	{
		float const fL = Length();
		if (fL > FLT_EPSILON)
		{
			float const fInvL = 1.0f / fL;
			Vec3 v;
			v.m_fX = m_fX * fInvL;
			v.m_fY = m_fY * fInvL;
			v.m_fZ = m_fZ * fInvL;
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
			m_fX *= fInvL;
			m_fY *= fInvL;
			m_fZ *= fInvL;
		}
	}

	float operator[](int nIndex) const { return ((float*)&m_fX)[nIndex]; }
	float & operator[](int nIndex) { return ((float*)&m_fX)[nIndex]; }

	Vec3 operator-() const
	{
		Vec3 v;
		v.m_fX = -m_fX;
		v.m_fY = -m_fY;
		v.m_fZ = -m_fZ;
		return v;
	}

	Vec3 operator*=(Vec3 const & v2)
	{
		m_fX *= v2.m_fX;
		m_fY *= v2.m_fY;
		m_fZ *= v2.m_fZ;
	}
	
	Vec3 operator+=(Vec3 const & v2)
	{
		m_fX += v2.m_fX;
		m_fY += v2.m_fY;
		m_fZ += v2.m_fZ;
	}

	Vec3 operator-=(Vec3 const & v2)
	{
		m_fX -= v2.m_fX;
		m_fY -= v2.m_fY;
		m_fZ -= v2.m_fZ;
	}
};

V6_INLINE Vec3 Vec3_Make( float x, float y, float z )
{
	Vec3 v;
	v.m_fX = x;
	v.m_fY = y;
	v.m_fZ = z;

	return v;
}

V6_INLINE float Dot(Vec3 const & v1, Vec3 const & v2)
{
	return v1.m_fX * v2.m_fX + v1.m_fY * v2.m_fY + v1.m_fZ * v2.m_fZ;
}

V6_INLINE Vec3 Cross(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = v1.m_fY * v2.m_fZ - v1.m_fZ * v2.m_fY;
	v.m_fY = v1.m_fZ * v2.m_fX - v1.m_fX * v2.m_fZ;
	v.m_fZ = v1.m_fX * v2.m_fY - v1.m_fY * v2.m_fX;
	return v;
}

V6_INLINE Vec3 Min(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = Min(v1.m_fX, v2.m_fX);
	v.m_fY = Min(v1.m_fY, v2.m_fY);
	v.m_fZ = Min(v1.m_fZ, v2.m_fZ);
	return v;
}

V6_INLINE Vec3 Max(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = Max(v1.m_fX, v2.m_fX);
	v.m_fY = Max(v1.m_fY, v2.m_fY);
	v.m_fZ = Max(v1.m_fZ, v2.m_fZ);
	return v;
}

V6_INLINE Vec3 operator*(Vec3 const & v1, float f)
{
	Vec3 v;
	v.m_fX = v1.m_fX * f;
	v.m_fY = v1.m_fY * f;
	v.m_fZ = v1.m_fZ * f;
	return v;
}

V6_INLINE Vec3 operator+(Vec3 const & v1, float f)
{
	Vec3 v;
	v.m_fX = v1.m_fX + f;
	v.m_fY = v1.m_fY + f;
	v.m_fZ = v1.m_fZ + f;
	return v;
}

V6_INLINE Vec3 operator-(Vec3 const & v1, float f)
{
	Vec3 v;
	v.m_fX = v1.m_fX - f;
	v.m_fY = v1.m_fY - f;
	v.m_fZ = v1.m_fZ - f;
	return v;
}

V6_INLINE Vec3 operator-(float f, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = f - v2.m_fX;
	v.m_fY = f - v2.m_fY;
	v.m_fZ = f - v2.m_fZ;
	return v;
}

V6_INLINE Vec3 operator*(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = v1.m_fX * v2.m_fX;
	v.m_fY = v1.m_fY * v2.m_fY;
	v.m_fZ = v1.m_fZ * v2.m_fZ;
	return v;
}

V6_INLINE Vec3 operator+(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = v1.m_fX + v2.m_fX;
	v.m_fY = v1.m_fY + v2.m_fY;
	v.m_fZ = v1.m_fZ + v2.m_fZ;
	return v;
}

V6_INLINE Vec3 operator-(Vec3 const & v1, Vec3 const & v2)
{
	Vec3 v;
	v.m_fX = v1.m_fX - v2.m_fX;
	v.m_fY = v1.m_fY - v2.m_fY;
	v.m_fZ = v1.m_fZ - v2.m_fZ;
	return v;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_VEC3_H__