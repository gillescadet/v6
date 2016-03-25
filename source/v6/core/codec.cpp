/*V6*/

#include <v6/core/common.h>

#include <v6/core/memory.h>
#include <v6/core/stream.h>

#include <v6/core/codec.h>

BEGIN_V6_CORE_NAMESPACE

struct CodecStreamHeader_s
{
	char				magic[4];
	u32					version;
	u32					size;
	CodecStreamDesc_s	desc;
};

struct CodecRawFrameHeader_s
{
	char				magic[4];
	u32					version;
	u32					size;
	CodecRawFrameDesc_s	desc;
};

struct CodecIFrameHeader_s
{
	char				magic[4];
	u32					version;
	u32					size;
	CodecIFrameDesc_s	desc;
};

struct CodecPFrameHeader_s
{
	char				magic[4];
	u32					version;
	u32					size;
	CodecPFrameDesc_s	desc;
};

u32 Codec_GetMipCount( float gridScaleMin, float gridScaleMax )
{
	return 1 + u32( ceil( log2f( gridScaleMax / gridScaleMin ) ) );
}

void Codec_WriteStreamHeader( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc )
{
	const u32 beginPos = streamWriter->GetPos();

	CodecStreamHeader_s streamHeader = {};
	memcpy( streamHeader.magic, CODEC_STREAM_MAGIC, 4 );
	streamHeader.version = CODEC_STREAM_VERSION;
	streamHeader.size = sizeof( CodecStreamHeader_s );
	memcpy( &streamHeader.desc, desc, sizeof( streamHeader.desc ) );

	streamWriter->Write( &streamHeader, sizeof( CodecStreamHeader_s ) );

	V6_ASSERT( streamWriter->GetPos() - beginPos == streamHeader.size );
}

void Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	u32 blockPosSize = 0;
	u32 blockDataSize = 0;

	u32 cellPerBucketCount = 4;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;
		blockDataSize += desc->blockCounts[bucket] * cellPerBucketCount * 4;
	}

	CodecRawFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_RAWFRAME_MAGIC, 4 );
	frameHeader.version = CODEC_RAWFRAME_VERSION;
	frameHeader.size = sizeof( CodecRawFrameHeader_s ) + blockPosSize + blockDataSize;
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, sizeof( CodecRawFrameHeader_s ) );
	streamWriter->Write( data->blockPos, blockPosSize );
	streamWriter->Write( data->blockData, blockDataSize );

	V6_ASSERT( streamWriter->GetPos() - beginPos == frameHeader.size );
}

bool Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, IAllocator* allocator )
{
	if ( streamReader->GetRemaining() < sizeof( CodecRawFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return false;
	}

	CodecRawFrameHeader_s frameHeader = {};
	streamReader->Read( sizeof( CodecRawFrameHeader_s ), &frameHeader );

	if ( memcmp( frameHeader.magic, CODEC_RAWFRAME_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for frame header.\n", frameHeader.magic[0], frameHeader.magic[1], frameHeader.magic[2], frameHeader.magic[3] );
		return false;
	}

	if ( frameHeader.version != CODEC_RAWFRAME_VERSION )
	{
		V6_ERROR( "Incompatible version %d for frame header.\n", frameHeader.version );
		return false;
	}

	u32 blockPosSize = 0;
	u32 blockDataSize = 0;

	u32 cellPerBucketCount = 4;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += frameHeader.desc.blockCounts[bucket] * 4;
		blockDataSize += frameHeader.desc.blockCounts[bucket] * cellPerBucketCount * 4;
	}

	if ( frameHeader.size != sizeof( CodecRawFrameHeader_s ) + blockPosSize + blockDataSize )
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

void Codec_WriteIFrame( IStreamWriter* streamWriter, const CodecIFrameDesc_s* desc, const CodecIFrameData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	u32 blockRangeSize = 0;
	u32 blockPosSize = 0;
	u32 blockDataSize = 0;
	u32 blockGroupSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockRangeSize += desc->rangeCounts[bucket] * 2;
		
		blockPosSize += desc->dataBlockCounts[bucket] * 4;

		const u32 cellPerBucketCount = 1 << (2 + bucket);
		blockDataSize += desc->dataBlockCounts[bucket] * cellPerBucketCount * 4;

		V6_ASSERT( (desc->usedBlockCounts[bucket] & 0x3F) == 0 );
		blockGroupSize += (desc->usedBlockCounts[bucket] / 64) * 2;
	}

	CodecIFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_IFRAME_MAGIC, 4 );
	frameHeader.version = CODEC_IFRAME_VERSION;
	frameHeader.size = sizeof( CodecIFrameHeader_s ) + blockRangeSize + blockPosSize + blockDataSize + blockGroupSize;
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, sizeof( CodecIFrameHeader_s ) );
	streamWriter->Write( data->ranges, blockRangeSize );
	streamWriter->Write( data->blockPos, blockPosSize );
	streamWriter->Write( data->blockData, blockDataSize );
	streamWriter->Write( data->groups, blockGroupSize );

	V6_ASSERT( streamWriter->GetPos() - beginPos == frameHeader.size );
}

void Codec_WritePFrame( IStreamWriter* streamWriter, const CodecPFrameDesc_s* desc, const CodecPFrameData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	u32 blockPosSize = 0;
	u32 blockDataSize = 0;
	u32 blockGroupSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockPosSize += desc->dataBlockCounts[bucket] * 4;

		const u32 cellPerBucketCount = 1 << (2 + bucket);
		blockDataSize += desc->dataBlockCounts[bucket] * cellPerBucketCount * 4;

		V6_ASSERT( (desc->usedBlockCounts[bucket] & 0x3F) == 0 );
		blockGroupSize += (desc->usedBlockCounts[bucket] / 64) * 2;
	}

	CodecPFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_PFRAME_MAGIC, 4 );
	frameHeader.version = CODEC_PFRAME_VERSION;
	frameHeader.size = sizeof( CodecPFrameHeader_s ) + blockPosSize + blockDataSize + blockGroupSize;
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, sizeof( CodecPFrameHeader_s ) );
	streamWriter->Write( data->blockPos, blockPosSize );
	streamWriter->Write( data->blockData, blockDataSize );
	streamWriter->Write( data->groups, blockGroupSize );

	V6_ASSERT( streamWriter->GetPos() - beginPos == frameHeader.size );
}

END_V6_CORE_NAMESPACE
