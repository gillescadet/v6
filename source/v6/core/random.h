/*V6*/

#pragma once

#ifndef __V6_CORE_RANDOM_H__
#define __V6_CORE_RANDOM_H__

#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

inline void RandSeed( u32 seed )
{
	srand( seed );
}

inline float RandFloat()
{
	static float fInvRandMax = 1.0f / RAND_MAX;
	return rand() * fInvRandMax;
}

inline Vec3 RandSphere()
{
	// http://mathworld.wolfram.com/SpherePointPicking.html

	const float alpha = RandFloat() * V6_PI * 2.0f;
	const float u = RandFloat() * 2.0f - 1.0f;
	const float v = Sqrt( 1.0f - u * u );
	const float x = v * Cos( alpha );
	const float y = v * Sin( alpha );
	const float z = u;
	return Vec3_Make( x, y, z );
}

inline u32 RandXORShift( u32 rng_state )
{
	// http://www.reedbeta.com/blog/2013/01/12/quick-and-easy-gpu-random-numbers-in-d3d11/

	// Xorshift algorithm from George Marsaglia's paper
	rng_state ^= (rng_state << 13);
	rng_state ^= (rng_state >> 17);
	rng_state ^= (rng_state << 5);
	
	return rng_state;
}

inline u32 HashU32( u32 seed )
{
	// http://www.reedbeta.com/blog/2013/01/12/quick-and-easy-gpu-random-numbers-in-d3d11/

	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	return seed;
}

inline u32 HashU64( u64 seed )
{
	const u32 seedl = HashU32( seed & 0xFFFFFFFF );
	const u32 seedh = HashU32( seed >> 32 );
	return seedl ^ seedh;
}

inline u32 HashPointer( void* p )
{
	return HashU64( (u64)p );
}

template < typename TYPE >
inline u16 Hash_MakeU16( TYPE value )
{
	switch ( sizeof( TYPE ) )
	{
	case 1:
	case 2:
	case 4:
		{
			const u32 hash = HashU32( value );
			const u32 hashl = (hash >>  0) & 0xFFFF;
			const u32 hashh = (hash >> 16) & 0xFFFF;
			return (u16)(hashl ^ hashh);
		}
		break;
	case 8:
		{
			const u16 hashl = Hash_MakeU16( (u32)((value >>  0) & 0xFFFFFFFF ) );
			const u16 hashh = Hash_MakeU16( (u32)((value >> 32) & 0xFFFFFFFF ) );
			return hashl ^ hashh;
		}
		break;
	default:
		V6_STATIC_ASSERT( !"unsupported" );
	}
}

inline u32 HashVector( Vec3 v )
{
	const u32 seedx = HashU32( (u32)v.x );
	const u32 seedy = HashU32( (u32)v.y );
	const u32 seedz = HashU32( (u32)v.z );
	return seedx ^ seedy ^ seedy;
}

template < u32 BASE >
inline float HaltonSequence( u32 index )
{
	// see wikipedia

	float r = 0.0;
	
	float f = 1.0f;
	u32 i = index + 1;
	while ( i > 0 )
	{
		f *= 1.0f / BASE;
		r += f * (i % BASE);
		i /= BASE;
	}

	return r;
}

END_V6_NAMESPACE

#endif // __V6_CORE_RANDOM_H__
