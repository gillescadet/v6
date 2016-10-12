/*V6*/

#pragma once

#ifndef __V6_CODEC_DECODER_H__
#define __V6_CODEC_DECODER_H__

#include <v6/codec/codec.h>

BEGIN_V6_NAMESPACE

class IAllocator;
class IStack;

struct VideoSequence_s
{
	CodecSequenceDesc_s		desc;
	void*					buffer;
	CodecFrameDesc_s*		frameDescArray;
	CodecFrameData_s*		frameDataArray;
	void**					frameBufferArray;
};

struct VideoStream_s
{
	char					name[256];
	CodecStreamDesc_s		desc;
	CodecStreamData_s		data;
	void*					buffer;
	VideoSequence_s*		sequences;
	u32*					frameOffets;
};

bool	VideoSequence_Load( VideoSequence_s* sequence, IStreamReader* streamReader, u32 sequenceID, IAllocator* allocator, IStack* stack );
void	VideoSequence_Release( VideoSequence_s* sequence, IAllocator* allocator );

u32		VideoStream_FindSequenceIDFromFrameID( const VideoStream_s* stream, u32 frameID );
bool	VideoStream_LoadDesc( const char* streamFilename, CodecStreamDesc_s* streamDesc );
bool	VideoStream_Load( VideoStream_s* stream, const char* streamFilename, IAllocator* allocator, IStack* stack );
void	VideoStream_Release( VideoStream_s* stream, IAllocator* allocator );
bool	VideoStream_Validate( const VideoStream_s* stream, const char* templateFilename, u32 frameOffset, IAllocator* allocator );

END_V6_NAMESPACE

#endif // __V6_CODEC_DECODER_H__
