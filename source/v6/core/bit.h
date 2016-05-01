/*V6*/

#pragma once

#ifndef __V6_CORE_BIT_H__
#define __V6_CORE_BIT_H__

#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

struct BitSet_s
{
	u32*	bits;
	u32		count;
	u32		findCurChunk;
	u32		findLastChunk;
	u32		findBitMask;
};

V6_INLINE u32 Bit_GetBitHighCount( u16 value )
{
	return (u32)__popcnt16( value );
}

V6_INLINE u32 Bit_GetBitHighCount( u32 value )
{
	return (u32)__popcnt( value );
}

V6_INLINE u32 Bit_GetBitHighCount( u64 value )
{
	return (u32)__popcnt64( value );
}

V6_INLINE u32 Bit_GetFirstBitHigh( u32 value )
{
	unsigned long index;
	return _BitScanReverse( &index, value ) == 0 ? (u32)-1 : (u32)index;
}

V6_INLINE u32 Bit_GetFirstBitHigh( u64 value )
{
	unsigned long index;
	return _BitScanReverse64( &index, value ) == 0 ? (u32)-1 : (u32)index;
}

V6_INLINE u32 BitSet_GetSize( u32 count )
{
	return (count + 31) >> 5;
}

V6_INLINE void BitSet_Init( BitSet_s* bitSet, u32* bits, u32 count )
{
	bitSet->bits = bits;
	bitSet->count = count;
	bitSet->findCurChunk = (u32)-1;
	bitSet->findLastChunk = (u32)-1;
}

V6_INLINE void BitSet_Clear( BitSet_s* bitSet )
{
	memset( bitSet->bits, 0, BitSet_GetSize( bitSet->count ) * sizeof( u32 ) );
}

V6_INLINE void BitSet_SetBit( BitSet_s* bitSet, u32 id )
{
	V6_ASSERT( id < bitSet->count );
	const u32 chunk = id >> 5;
	const u32 bit = id & 0x1F;
	bitSet->bits[chunk] |= 1 << bit;
}

V6_INLINE void BitSet_UnsetBit( BitSet_s* bitSet, u32 id )
{
	V6_ASSERT( id < bitSet->count );
	const u32 chunk = id >> 5;
	const u32 bit = id & 0x1F;
	bitSet->bits[chunk] &= ~(1 << bit);
}

V6_INLINE bool BitSet_GetBit( BitSet_s* bitSet, u32 id )
{
	V6_ASSERT( id < bitSet->count );
	const u32 chunk = id >> 5;
	const u32 bit = id & 0x1F;
	return (bitSet->bits[chunk] & (1 << bit)) != 0;
}

V6_INLINE void BitSet_FindBegin( BitSet_s* bitSet )
{
	bitSet->findCurChunk = 0;
	bitSet->findLastChunk = Max( 0, (int)BitSet_GetSize( bitSet->count )-1 );
	bitSet->findBitMask = bitSet->count > 0 ? bitSet->bits[0] : 0;
}

V6_INLINE bool BitSet_FindNext( BitSet_s* bitSet, u32* id )
{
	V6_ASSERT( bitSet->findCurChunk != (u32)-1 );

	for (;;)
	{
		const u32 bit = Bit_GetFirstBitHigh( bitSet->findBitMask );
		if ( bit != (u32)-1 )
		{
			bitSet->findBitMask -= 1 << bit;
			*id = (bitSet->findCurChunk << 5) + bit;
			return true;
		}
		
		if ( bitSet->findCurChunk == bitSet->findLastChunk )
			return false;
		
		++bitSet->findCurChunk;
		bitSet->findBitMask = bitSet->bits[bitSet->findCurChunk];
	}
}

END_V6_NAMESPACE

#endif // __V6_CORE_BIT_H__
