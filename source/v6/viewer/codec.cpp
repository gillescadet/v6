/*V6*/

#include <v6/viewer/common.h>


#include <v6/core/memory.h>
#include <v6/core/stream.h>

#include <v6/viewer/codec.h>

BEGIN_V6_VIEWER_NAMESPACE

struct CodecStreamHeader_s
{
	char		magic[4];
	core::u32	version;
	core::u32	size;
	core::u32	frameCount;
};

struct CodecFrameHeader_s
{
	char				magic[4];
	core::u32			version;
	core::u32			size;
	CodecFrameDesc_s	desc;
};

bool Codec_ReadFrame( core::IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, core::IAllocator* allocator )
{
	if ( streamReader->GetRemaining() < sizeof( CodecFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return false;
	}

	CodecFrameHeader_s frameHeader = {};
	streamReader->Read( sizeof( CodecFrameHeader_s ), &frameHeader );

	if ( memcmp( frameHeader.magic, CODEC_FRAME_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for frame header.\n", frameHeader.magic[0], frameHeader.magic[1], frameHeader.magic[2], frameHeader.magic[3] );
		return false;
	}

	if ( frameHeader.version != CODEC_FRAME_VERSION )
	{
		V6_ERROR( "Incompatible version %d for frame header.\n", frameHeader.version );
		return false;
	}

	if ( frameHeader.version != CODEC_FRAME_VERSION )
	{
		V6_ERROR( "Incompatible version %d for frame header.\n", frameHeader.version );
		return false;
	}

	core::u32 blockPosSize = 0;
	core::u32 blockDataSize = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += frameHeader.desc.blockCounts[bucket] * 4;
		blockDataSize += frameHeader.desc.blockCounts[bucket] * cellPerBucketCount * 4;
	}

	if ( frameHeader.size != sizeof( CodecFrameHeader_s ) + blockPosSize + blockDataSize )
	{
		V6_ERROR( "Bad file size of %d bytes for frame header.\n", frameHeader.size );
		return false;
	}

	memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );

	if ( blockPosSize == 0 )
	{
		data->blockPos = nullptr;
		data->blockData = nullptr;
		V6_ASSERT( blockDataSize == 0 );
		return true;
	}

	if ( streamReader->GetRemaining() < (int)blockPosSize )
	{
		V6_ERROR( "Stream is too small to contain the frame block pos.\n" );
		return false;
	}

	data->blockPos = allocator->alloc( blockPosSize );

	if ( data->blockPos == nullptr )
	{
		V6_ERROR( "Not enough memory to allocate frame block pos of %d bytes.\n", blockPosSize );
		return false;
	}

	streamReader->Read( blockPosSize, data->blockPos );

	if ( streamReader->GetRemaining() < (int)blockDataSize )
	{
		V6_ERROR( "Stream is too small to contain the frame block data.\n" );
		return false;
	}

	data->blockData = allocator->alloc( blockDataSize );

	if ( data->blockData == nullptr )
	{
		V6_ERROR( "Not enough memory to allocate frame block data of %d bytes.\n", blockDataSize );
		return false;
	}

	streamReader->Read( blockDataSize, data->blockData );
	
	return true;
}

void Codec_WriteFrame( core::IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data )
{
	const core::u32 beginPos = streamWriter->GetPos();

	core::u32 blockPosSize = 0;
	core::u32 blockDataSize = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;
		blockDataSize += desc->blockCounts[bucket] * cellPerBucketCount * 4;
	}

	CodecFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
	frameHeader.version = CODEC_FRAME_VERSION;
	frameHeader.size = sizeof( CodecFrameHeader_s ) + blockPosSize + blockDataSize;
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, sizeof( CodecFrameHeader_s ) );
	streamWriter->Write( data->blockPos, blockPosSize );
	streamWriter->Write( data->blockData, blockDataSize );

	V6_ASSERT( streamWriter->GetSize() - beginPos == frameHeader.size );
}

END_V6_VIEWER_NAMESPACE
