/*V6*/

#pragma once

#ifndef __V6_CORE_DECODER_H__
#define __V6_CORE_DECODER_H__

#include <v6/codec/codec.h>

BEGIN_V6_NAMESPACE

class IAllocator;
class IStack;

struct Sequence_s
{
	CodecSequenceDesc_s		desc;
	CodecSequenceData_s		data;
	void*					buffer;
	CodecFrameDesc_s*		frameDescArray;
	CodecFrameData_s*		frameDataArray;
	void**					frameBufferArray;
};

bool Sequence_Load( const char* streamFilename, Sequence_s* sequence, IAllocator* allocator, IStack* stack );
bool Sequence_LoadDesc( const char* sequenceFilename, CodecSequenceDesc_s* sequenceDesc, IStack* stack );
void Sequence_Release( Sequence_s* sequence, IAllocator* allocator );
bool Sequence_Validate( const char* templateFilename, const char* sequenceFilename, const Sequence_s* sequence, IAllocator* allocator );

END_V6_NAMESPACE

#endif // __V6_CORE_DECODER_H__
