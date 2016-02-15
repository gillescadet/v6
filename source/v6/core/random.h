/*V6*/

#pragma once

#ifndef __V6_CORE_RANDOM_H__
#define __V6_CORE_RANDOM_H__

BEGIN_V6_CORE_NAMESPACE

inline float RandFloat()
{
	static float fInvRandMax = 1.0f / RAND_MAX;
	return rand() * fInvRandMax;
}

inline Vec3 RandSphere()
{
	// http://mathworld.wolfram.com/SpherePointPicking.html

	const float alpha = RandFloat() * PI * 2.0f;
	const float u = RandFloat() * 2.0f - 1.0f;
	const float v = Sqrt( 1.0f - u * u );
	const float x = v * Cos( alpha );
	const float y = v * Sin( alpha );
	const float z = u;
	return Vec3_Make( x, y, z );
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_RANDOM_H__