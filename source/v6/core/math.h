/*V6*/

#pragma once

#ifndef __V6_CORE_MATH_H__
#define __V6_CORE_MATH_H__

BEGIN_V6_CORE_NAMESPACE

static const float PI = 3.1415926f;

template<typename T>
V6_INLINE T Abs( T x ) { return fabs( x ); }

template<typename T>
V6_INLINE bool IsPowOfTwo( T x ) { return (x & (x-1)) == 0; }

template<typename T>
V6_INLINE T Max(T x, T y) { return x < y ? y : x; }
	
template<typename T>
V6_INLINE T Min(T x, T y) { return x < y ? x : y; }
	
template<typename T>
V6_INLINE T Clamp(T v, T min, T max) { return Max(min, Min(v, max)); }

V6_INLINE float Pow(float v, float e) { return pow(v, e); }

V6_INLINE float Sqrt(float v) { return sqrt(v); }

V6_INLINE void SinCos( float a, float* s, float* c) { *s = sin(a); *c = cos(a); }

V6_INLINE float DegToRad( float deg ) { return deg * (PI / 180.0f); }

V6_INLINE float RadToDeg( float rad ) { return rad * (180.0f / PI); }

template<typename T>
V6_INLINE void Swap( T& x, T& y )
{ 
	T tmp = x;
	x = y;
	y = tmp;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MATH_H__