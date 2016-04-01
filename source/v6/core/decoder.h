/*V6*/

#pragma once

#ifndef __V6_CORE_DECODER_H__
#define __V6_CORE_DECODER_H__

#include <v6/core/codec.h>

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

struct Sequence_s
{
	CodecSequenceDesc_s		desc;
	CodecSequenceData_s		data;
	void*					buffer;
	CodecFrameDesc_s*		frameDescArray;
	CodecFrameData_s*		frameDataArray;
	void**					frameBufferArray;
};

bool Sequence_Load( const char* streamFilename, Sequence_s* sequence, IAllocator* allocator );
void Sequence_Release( Sequence_s* sequence, IAllocator* allocator );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_DECODER_H__
