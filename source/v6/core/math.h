/*V6*/

#pragma once

#ifndef __V6_CORE_MATH_H__
#define __V6_CORE_MATH_H__

BEGIN_V6_NAMESPACE

#define V6_M_TO_CM		100.0f
#define V6_PI			3.1415926f

template< typename T >
V6_INLINE T Abs( T x ) { return abs( x ); }

template<>
V6_INLINE float Abs( float x ) { return fabsf( x ); }

template<typename T>
V6_INLINE T Clamp(T v, T min, T max) { return Max(min, Min(v, max)); }

template<typename T>
V6_INLINE T Cos( T x ) { return cosf( x ); }

template<typename T>
V6_INLINE bool IsPowOfTwo( T x ) { return (x & (x-1)) == 0; }

template<typename T>
V6_INLINE constexpr bool IsPowOfTwo_ConstExpr( T x ) { return (x & (x-1)) == 0; }

template < u32 ALIGNMENT >
V6_INLINE bool IsAligned( u32 x ) { V6_STATIC_ASSERT( IsPowOfTwo_ConstExpr( ALIGNMENT ) ); return (x & (u32)(ALIGNMENT-1)) == 0; }

template < u32 ALIGNMENT >
V6_INLINE bool IsAligned( u64 x ) { V6_STATIC_ASSERT( IsPowOfTwo_ConstExpr( ALIGNMENT ) ); return (x & (u64)(ALIGNMENT-1)) == 0; }

template < u32 ALIGNMENT, typename T >
V6_INLINE bool IsAligned( const T* x ) { V6_STATIC_ASSERT( IsPowOfTwo_ConstExpr( ALIGNMENT ) ); return ((uintptr_t)x & (uintptr_t)(ALIGNMENT-1)) == 0; }

template<typename T1, typename T2>
V6_INLINE T1 Lerp(T1 v0, T1 v1, T2 t) { return (1-t) * v0 + t * v1; }

template<typename T>
V6_INLINE T Max(T x, T y) { return x < y ? y : x; }
	
template<typename T>
V6_INLINE T Min(T x, T y) { return x < y ? x : y; }
	
V6_INLINE float Pow(float v, float e) { return powf( v, e ); }

V6_INLINE float Sqrt(float v) { return sqrtf( v ); }

V6_INLINE void SinCos( float a, float* s, float* c) { *s = sinf( a ); *c = cosf( a ); }

V6_INLINE float DegToRad( float deg ) { return deg * (V6_PI / 180.0f); }

V6_INLINE float RadToDeg( float rad ) { return rad * (180.0f / V6_PI); }

template < u32 ALIGNMENT, typename T >
V6_INLINE T PowOfTwoRoundUp( T size )
{ 
	V6_ASSERT( IsPowOfTwo( ALIGNMENT ) );
	return (size + ALIGNMENT - 1) & ~((T)ALIGNMENT - 1);
}

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

END_V6_NAMESPACE

#endif // __V6_CORE_MATH_H__