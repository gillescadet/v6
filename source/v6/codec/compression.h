/*V6*/

#pragma once

#ifndef __V6_CORE_COMPRESSION_H__
#define __V6_CORE_COMPRESSION_H__

BEGIN_V6_NAMESPACE

struct EncodedBlock_s
{
	u32	cellEndColors;
	u64	cellPresence;
	u64	cellColorIndices;
};

struct EncodedBlockEx_s
{
	u32	cellEndColors;
	u64	cellPresence;
	u64	cellColorIndices[2];
};

void	Block_Decode( u32 cellRGBA[64], u32* cellCount, const EncodedBlockEx_s* encodedBlock );
u32		Block_Encode_BoundingBox( EncodedBlockEx_s* encodedBlock, const u32 cellRGBA[64], u32 cellCount );
u32		Block_Encode_Optimize( EncodedBlockEx_s* encodedBlock, const u32 cellRGBA[64], u32 cellCount, u32 quality );

END_V6_NAMESPACE

#endif // __V6_CORE_COMPRESSION_H__
