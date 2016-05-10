/*V6*/

#pragma once

#ifndef __V6_CORE_RANDOM_H__
#define __V6_CORE_RANDOM_H__

BEGIN_V6_NAMESPACE

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

inline u32 HashVector( Vec3 v )
{
	const u32 seedx = HashU32( (u32)v.x );
	const u32 seedy = HashU32( (u32)v.y );
	const u32 seedz = HashU32( (u32)v.z );
	return seedx ^ seedy ^ seedy;
}

END_V6_NAMESPACE

#endif // __V6_CORE_RANDOM_H__