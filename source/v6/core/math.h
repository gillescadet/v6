/*V6*/

#pragma once

#ifndef __V6_CORE_MATH_H__
#define __V6_CORE_MATH_H__

BEGIN_V6_CORE_NAMESPACE

class CMath
{
public:
	template<typename T>
	V6_INLINE static T Max(T x, T y) { return x < y ? y : x; }
	
	template<typename T>
	V6_INLINE static T Min(T x, T y) { return x < y ? x : y; }
	
	template<typename T>
	V6_INLINE static T Clamp(T v, T min, T max) { return Max(min, Min(v, max)); }
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MATH_H__