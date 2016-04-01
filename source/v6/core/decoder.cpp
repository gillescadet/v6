/*V6*/
#include <v6/core/common.h>

#include <v6/core/codec.h>
#include <v6/core/decoder.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_CORE_NAMESPACE

bool Sequence_Load( const char* sequenceFilename, Sequence_s* sequence, IAllocator* allocator )
{
	CFileReader fileReader;
	if ( !fileReader.Open( sequenceFilename ) )
	{
		V6_ERROR( "Unable to open file %s\n", sequenceFilename );
		return false;
	}

	sequence->buffer = Codec_ReadSequence( &fileReader, &sequence->desc, &sequence->data, allocator );
	if ( !sequence->buffer )
		return false;

	sequence->frameDescArray = allocator->newArray< CodecFrameDesc_s >( sequence->desc.frameCount );
	sequence->frameDataArray = allocator->newArray< CodecFrameData_s >( sequence->desc.frameCount );
	sequence->frameBufferArray = allocator->newArray< void* >( sequence->desc.frameCount );
	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		void* frameBuffer = Codec_ReadFrame( &fileReader, &sequence->frameDescArray[frameID], &sequence->frameDataArray[frameID], frameID, allocator );
		if ( !frameBuffer )
			return false;
		sequence->frameBufferArray[frameID] = frameBuffer;
	}

	if ( fileReader.GetRemaining() > 0 )
	{
		V6_ERROR( "Uncomplete read of file %s\n", sequenceFilename );
		return false;
	}

	return true;
}

void Sequence_Release( Sequence_s* sequence, IAllocator* allocator )
{
	V6_ASSERT( sequence->frameDescArray );
	V6_ASSERT( sequence->frameDataArray );
	V6_ASSERT( sequence->buffer );
	V6_ASSERT( sequence->frameBufferArray );

	allocator->free( sequence->buffer );
	allocator->deleteArray( sequence->frameDescArray );
	allocator->deleteArray( sequence->frameDataArray );
	allocator->deleteArray( sequence->frameBufferArray );
	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
		allocator->free( sequence->frameBufferArray[frameID] );
}

END_V6_CORE_NAMESPACE
