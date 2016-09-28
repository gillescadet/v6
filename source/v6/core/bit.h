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

struct BitStream_s
{
	u64*	buffer;
	u32		bitCount;
	u32		bitCapacity;
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
	return _BitScanForward( &index, value ) == 0 ? (u32)-1 : (u32)index;
}

V6_INLINE u32 Bit_GetFirstBitHigh( u64 value )
{
	unsigned long index;
	return _BitScanForward64( &index, value ) == 0 ? (u32)-1 : (u32)index;
}

V6_INLINE u32 Bit_GetFirstBitHighNonZero( u32 value )
{
	unsigned long index;
	_BitScanForward( &index, value );
	return (u32)index;
}

V6_INLINE u32 Bit_GetFirstBitHighNonZero( u64 value )
{
	unsigned long index;
	_BitScanForward64( &index, value );
	return (u32)index;
}

V6_INLINE u32 Bit_GetLastBitHigh( u32 value )
{
	unsigned long index;
	return _BitScanReverse( &index, value ) == 0 ? (u32)-1 : (u32)index;
}

V6_INLINE u32 Bit_GetLastBitHigh( u64 value )
{
	unsigned long index;
	return _BitScanReverse64( &index, value ) == 0 ? (u32)-1 : (u32)index;
}

V6_INLINE u32 Bit_GetLastBitHighNonZero( u32 value )
{
	unsigned long index;
	_BitScanReverse( &index, value );
	return (u32)index;
}

V6_INLINE u32 Bit_GetLastBitHighNonZero( u64 value )
{
	unsigned long index;
	_BitScanReverse64( &index, value );
	return (u32)index;
}

#define BitSet_GetSize( count ) ( (count + 31) >> 5 )

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

V6_INLINE void BitStream_Align( BitStream_s* bitStream )
{
	const u32 offset = bitStream->bitCount & 63;
	if ( offset == 0 )
		return;

	const u32 remaining = 64 - offset;
	bitStream->bitCount += remaining;
}

V6_INLINE u64* BitStream_GetBuffer( BitStream_s* bitStream )
{
	const u32 pos = bitStream->bitCount >> 6;
	return bitStream->buffer + pos;
}

V6_INLINE u32 BitStream_GetSize( BitStream_s* bitStream )
{
	const u32 pos = (bitStream->bitCount + 63) >> 6;
	return pos * 8;
}

V6_INLINE void BitStream_InitForWrite( BitStream_s* bitStream, u64* buffer, u32 bitCapacity )
{
	bitStream->buffer = buffer;
	bitStream->bitCount = 0;
	bitStream->bitCapacity = bitCapacity;
	memset( bitStream->buffer, 0, ((bitCapacity + 63) / 64) * 8 );
}

V6_INLINE void BitStream_InitForRead( BitStream_s* bitStream, u64* buffer, u32 bitCapacity )
{
	bitStream->buffer = buffer;
	bitStream->bitCount = 0;
	bitStream->bitCapacity = bitCapacity;
}

V6_INLINE bool BitStream_IsAligned( BitStream_s* bitStream )
{
	const u32 offset = bitStream->bitCount & 63;
	return offset == 0;
}

template < u32 BIT_COUNT >
V6_INLINE void BitStream_Read( u32* value, BitStream_s* bitStream )
{
	V6_ASSERT( bitStream->bitCount + BIT_COUNT <= bitStream->bitCapacity );

	const u32 pos = bitStream->bitCount >> 6;
	const u32 offset = bitStream->bitCount & 63;
	u64 bits = bitStream->buffer[pos+0] >> offset;
	const u32 remaining = 64 - offset;
	bits |= (BIT_COUNT > remaining) ? (bitStream->buffer[pos+1] << remaining) : 0;
	bits &= (1ull << BIT_COUNT)-1;
	bitStream->bitCount += BIT_COUNT;

	*value = (u32)bits;
}

template < u32 BIT_COUNT >
V6_INLINE void BitStream_Read( u64* value, BitStream_s* bitStream )
{
	V6_ASSERT( bitStream->bitCount + BIT_COUNT <= bitStream->bitCapacity );

	const u32 pos = bitStream->bitCount >> 6;
	const u32 offset = bitStream->bitCount & 63;
	u64 bits = bitStream->buffer[pos+0] >> offset;
	const u32 remaining = 64 - offset;
	bits |= (BIT_COUNT > remaining) ? (bitStream->buffer[pos+1] << remaining) : 0;
	bits &= (1ull << BIT_COUNT)-1;
	bitStream->bitCount += BIT_COUNT;

	*value = bits;
}

V6_INLINE void BitStream_SkipByteCount( BitStream_s* bitStream, u32 byteCount )
{
	bitStream->bitCount += byteCount * 8;
}

template < u32 BIT_COUNT >
V6_INLINE void BitStream_Write( BitStream_s* bitStream, u32 value )
{
	V6_ASSERT( bitStream->bitCount + BIT_COUNT <= bitStream->bitCapacity );
	V6_STATIC_ASSERT( BIT_COUNT > 0 && BIT_COUNT <= 32 );

	const u32 pos = bitStream->bitCount >> 6;
	const u32 offset = bitStream->bitCount & 63;
	bitStream->buffer[pos+0] |= (u64)value << offset;
	const u32 remaining = 64 - offset;
	bitStream->buffer[pos+1] |= (BIT_COUNT > remaining) ? ((u64)value >> remaining) : 0;
	bitStream->bitCount += BIT_COUNT;
}

template < u32 BIT_COUNT >
V6_INLINE void BitStream_Write( BitStream_s* bitStream, u64 value )
{
	V6_ASSERT( bitStream->bitCount + BIT_COUNT <= bitStream->bitCapacity );
	V6_STATIC_ASSERT( BIT_COUNT > 32 && BIT_COUNT <= 64 );

	const u32 pos = bitStream->bitCount >> 6;
	const u32 offset = bitStream->bitCount & 63;
	bitStream->buffer[pos+0] |= value << offset;
	const u32 remaining = 64 - offset;
	bitStream->buffer[pos+1] |= (BIT_COUNT > remaining) ? (value >> remaining) : 0;
	bitStream->bitCount += BIT_COUNT;
}

END_V6_NAMESPACE

#endif // __V6_CORE_BIT_H__
