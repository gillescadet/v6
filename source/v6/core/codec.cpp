/*V6*/

#include <v6/core/common.h>

#include <v6/core/codec.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_CORE_NAMESPACE

struct CodecSequenceHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecSequenceDesc_s		desc;
};

struct CodecRawFrameHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecRawFrameDesc_s		desc;
};

struct CodecFrameHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecFrameDesc_s		desc;
};

Vec3 Codec_ComputeGridCenter( const Vec3* pos, float gridScale, core::u32 gridMacroHalfWidth )
{
	// note: use the exact same code than Codec_ComputeGridCenter
	const float blockSize = gridMacroHalfWidth / gridScale;
	const Vec3 normalizedPos = *pos * (1.0f / blockSize);

	return Vec3_Make( floorf( normalizedPos.x ), floorf( normalizedPos.y ), floorf( normalizedPos.z ) ) * blockSize;
}

Vec3i Codec_ComputeMacroGridCoords( const Vec3* pos, float gridScale, core::u32 gridMacroHalfWidth )
{
	// note: use the exact same code than Codec_ComputeMacroGridCoords
	const float blockSize = gridMacroHalfWidth / gridScale;
	const Vec3 normalizedPos = *pos * (1.0f / blockSize);

	return Vec3i_Make( (int)floorf( normalizedPos.x ), (int)floorf( normalizedPos.y ), (int)floorf( normalizedPos.z ) );
}

u32 Codec_GetMipCount( float gridScaleMin, float gridScaleMax )
{
	return 1 + u32( ceil( log2f( gridScaleMax / gridScaleMin ) ) );
}

void* Codec_ReadSequence( IStreamReader* streamReader, CodecSequenceDesc_s* desc, CodecSequenceData_s* data, IAllocator* allocator )
{	
	const u32 beginPos = streamReader->GetPos();

	if ( streamReader->GetRemaining() < sizeof( CodecSequenceHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the sequence header.\n" );
		return false;
	}

	CodecSequenceHeader_s sequenceHeader = {};
	streamReader->Read( sizeof( CodecSequenceHeader_s ), &sequenceHeader );

	if ( memcmp( sequenceHeader.magic, CODEC_SEQUENCE_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for sequence header.\n", sequenceHeader.magic[0], sequenceHeader.magic[1], sequenceHeader.magic[2], sequenceHeader.magic[3] );
		return false;
	}

	if ( sequenceHeader.version != CODEC_SEQUENCE_VERSION )
	{
		V6_ERROR( "Incompatible version %d for sequence header.\n", sequenceHeader.version );
		return false;
	}

	u32 rangeSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		rangeSize += sequenceHeader.desc.rangeCounts[bucket] * 2;

	if ( sequenceHeader.size != sizeof( CodecSequenceHeader_s ) + rangeSize )
	{
		V6_ERROR( "Bad file size of %d bytes for sequence header.\n", sequenceHeader.size );
		return false;
	}

	if ( (u32)streamReader->GetRemaining() < rangeSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for frame header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &sequenceHeader.desc, sizeof( sequenceHeader.desc ) );

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( rangeSize );

	streamReader->Read( rangeSize, buffer );

	V6_ASSERT( streamReader->GetPos() - beginPos == sequenceHeader.size );

	return buffer;
}

void Codec_WriteSequence( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc, const CodecSequenceData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	u32 rangeSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		rangeSize += desc->rangeCounts[bucket] * 2;

	CodecSequenceHeader_s sequenceHeader = {};
	memcpy( sequenceHeader.magic, CODEC_SEQUENCE_MAGIC, 4 );
	sequenceHeader.version = CODEC_SEQUENCE_VERSION;
	sequenceHeader.size = sizeof( CodecSequenceHeader_s ) + rangeSize;
	memcpy( &sequenceHeader.desc, desc, sizeof( sequenceHeader.desc ) );

	streamWriter->Write( &sequenceHeader, sizeof( CodecSequenceHeader_s ) );
	streamWriter->Write( data->ranges, rangeSize );
	
	V6_ASSERT( streamWriter->GetPos() - beginPos == sequenceHeader.size );
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

bool Codec_ReadRawFrameHeader( IStreamReader* streamReader, CodecRawFrameDesc_s* desc )
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
	
	if ( streamReader->GetRemaining() < (int)blockPosSize )
	{
		V6_ERROR( "Stream is too small to contain the frame block pos.\n" );
		return false;
	}
	
	return true;
}

bool Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, IAllocator* allocator )
{
	if ( !Codec_ReadRawFrameHeader( streamReader, desc ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return false;
	}

	u32 blockPosSize = 0;
	u32 blockDataSize = 0;

	u32 cellPerBucketCount = 4;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;
		blockDataSize += desc->blockCounts[bucket] * cellPerBucketCount * 4;
	}

	if ( blockPosSize == 0 )
	{
		data->blockPos = nullptr;
		data->blockData = nullptr;
		V6_ASSERT( blockDataSize == 0 );
		return true;
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

void* Codec_ReadFrame( IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameID, IAllocator* allocator )
{
	const u32 beginPos = streamReader->GetPos();

	if ( streamReader->GetRemaining() < sizeof( CodecFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return nullptr;
	}

	CodecFrameHeader_s frameHeader = {};
	streamReader->Read( sizeof( CodecFrameHeader_s ), &frameHeader );

	if ( memcmp( frameHeader.magic, CODEC_FRAME_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for frame header.\n", frameHeader.magic[0], frameHeader.magic[1], frameHeader.magic[2], frameHeader.magic[3] );
		return nullptr;
	}

	if ( frameHeader.version != CODEC_FRAME_VERSION )
	{
		V6_ERROR( "Incompatible version %d for frame header.\n", frameHeader.version );
		return nullptr;
	}

	if ( frameHeader.desc.frameID != frameID )
	{
		V6_ERROR( "Incompatible frame ID %d for frame desc.\n", frameHeader.desc.frameID );
		return nullptr;
	}

	u32 blockPosSize = 0;
	u32 blockDataSize = 0;
	u32 rangeIDSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockPosSize += frameHeader.desc.blockCounts[bucket] * 4;

		const u32 cellPerBucketCount = 1 << (2 + bucket);
		blockDataSize += frameHeader.desc.blockCounts[bucket] * cellPerBucketCount * 4;

		rangeIDSize += frameHeader.desc.blockRangeCounts[bucket] * 2;
	}

	const u32 chunkSize = blockPosSize + blockDataSize + rangeIDSize;

	if ( frameHeader.size != sizeof( CodecFrameHeader_s ) + chunkSize )
	{
		V6_ERROR( "Bad file size of %d bytes for frame header.\n", frameHeader.size );
		return nullptr;
	}

	if ( (u32)streamReader->GetRemaining() < chunkSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for frame header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( chunkSize );
	u8* chunk = buffer;

	streamReader->Read( blockPosSize, chunk );
	data->blockPos = (u32*)chunk;
	chunk += blockPosSize;

	streamReader->Read( blockDataSize, chunk );
	data->blockData = (u32*)chunk;
	chunk += blockDataSize;
	
	streamReader->Read( rangeIDSize, chunk );
	data->rangeIDs = (u16*)chunk;
	chunk += rangeIDSize;
	
	V6_ASSERT( streamReader->GetPos() - beginPos == frameHeader.size );
	V6_ASSERT( (u32)(chunk - buffer) == chunkSize );

	return buffer;
}

void Codec_WriteFrame( IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	u32 blockPosSize = 0;
	u32 blockDataSize = 0;
	u32 rangeIDSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;

		const u32 cellPerBucketCount = 1 << (2 + bucket);
		blockDataSize += desc->blockCounts[bucket] * cellPerBucketCount * 4;

		V6_ASSERT( desc->blockRangeCounts[bucket] <= CODEC_RANGE_MAX_COUNT )
		rangeIDSize += desc->blockRangeCounts[bucket] * 2;
	}

	CodecFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
	frameHeader.version = CODEC_FRAME_VERSION;
	frameHeader.size = sizeof( CodecFrameHeader_s ) + blockPosSize + blockDataSize + rangeIDSize;
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, sizeof( CodecFrameHeader_s ) );
	streamWriter->Write( data->blockPos, blockPosSize );
	streamWriter->Write( data->blockData, blockDataSize );
	streamWriter->Write( data->rangeIDs, rangeIDSize );

	V6_ASSERT( streamWriter->GetPos() - beginPos == frameHeader.size );
}

END_V6_CORE_NAMESPACE
