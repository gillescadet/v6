/*V6*/

#pragma once

#ifndef __V6_CORE_MATH_H__
#define __V6_CORE_MATH_H__

BEGIN_V6_CORE_NAMESPACE

static const float PI = 3.1415926f;

static const float M_TO_CM = 100.0f;

template< typename T >
V6_INLINE T Abs( T x ) { return abs( x ); }

template<typename T>
V6_INLINE T Clamp(T v, T min, T max) { return Max(min, Min(v, max)); }

template<typename T>
V6_INLINE T Cos( T x ) { return cosf( x ); }

template<typename T>
V6_INLINE bool IsPowOfTwo( T x ) { return (x & (x-1)) == 0; }

template<typename T>
V6_INLINE T Max(T x, T y) { return x < y ? y : x; }
	
template<typename T>
V6_INLINE T Min(T x, T y) { return x < y ? x : y; }
	
V6_INLINE float Pow(float v, float e) { return powf( v, e ); }

V6_INLINE float Sqrt(float v) { return sqrtf( v ); }

V6_INLINE void SinCos( float a, float* s, float* c) { *s = sinf( a ); *c = cosf( a ); }

V6_INLINE float DegToRad( float deg ) { return deg * (PI / 180.0f); }

V6_INLINE float RadToDeg( float rad ) { return rad * (180.0f / PI); }

template<typename T>
V6_INLINE T Sin( T x ) { return sinf( x ); }

V6_INLINE float Tan(float v) { return tanf( v ); }

template<typename T>
V6_INLINE void Swap( T& x, T& y )
{ 
	T tmp = x;
	x = y;
	y = tmp;
}

template<typename T>
V6_INLINE T MulKB( T x ) { return x << 10; }

template<typename T>
V6_INLINE T MulMB( T x ) { return x << 20; }

template<typename T>
V6_INLINE T MulGB( T x ) { return x << 30; }

template<typename T>
V6_INLINE T DivKB( T x ) { return x >> 10; }

template<typename T>
V6_INLINE T DivMB( T x ) { return x >> 20; }

template<typename T>
V6_INLINE T DivGB( T x ) { return x >> 30; }

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MATH_H__