/*V6*/

#pragma once

#ifndef __V6_CORE_COMPRESSION_H__
#define __V6_CORE_COMPRESSION_H__

BEGIN_V6_CORE_NAMESPACE

struct EncodedBlock_s
{
	u32	cellEndColors;
	u64	cellPresence;
	u64	cellColorIndices[2];
};

struct DecodedBlock_s
{
	u32	cellRGBA[64];
	u32	cellCount;
};

DecodedBlock_s Block_Decode( EncodedBlock_s encodedBlock );
EncodedBlock_s Block_Encode( u32 cellRGBA[64], u32 cellCount );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_COMPRESSION_H__
