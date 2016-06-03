/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#if CODEC_FRAME_COMPRESS == 1
#include <lz4/lib/lz4.h>
#include <lz4/lib/lz4hc.h>
#endif // #if CODEC_FRAME_COMPRESS == 1

#define CODEC_LZ4_COMPRESSION_LEVEL 4

BEGIN_V6_NAMESPACE

struct CodecStreamHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecStreamDesc_s		desc;
};

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

Vec3 Codec_ComputeGridCenter( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth )
{
	// note: use the exact same code than Codec_ComputeGridCenter
	const float blockSize = gridScale / gridMacroHalfWidth;
	const Vec3 normalizedPos = *pos * (1.0f / blockSize);

	return Vec3_Make( floorf( normalizedPos.x ), floorf( normalizedPos.y ), floorf( normalizedPos.z ) ) * blockSize;
}

Vec3i Codec_ComputeMacroGridCoords( const Vec3* pos, float gridScale, u32 gridMacroHalfWidth )
{
	// note: use the exact same code than Codec_ComputeMacroGridCoords
	const float blockSize = gridScale / gridMacroHalfWidth;
	const Vec3 normalizedPos = *pos * (1.0f / blockSize);

	return Vec3i_Make( (int)floorf( normalizedPos.x ), (int)floorf( normalizedPos.y ), (int)floorf( normalizedPos.z ) );
}

u32 Codec_GetMipCount( float gridScaleMin, float gridScaleMax )
{
	return 1 + u32( ceil( log2f( gridScaleMax / gridScaleMin ) ) );
}

void* Codec_ReadStream( IStreamReader* streamReader, CodecStreamDesc_s* desc, CodecStreamData_s* data, IAllocator* allocator )
{
	const u32 beginPos = streamReader->GetPos();

	CodecStreamHeader_s streamHeader = {};

	if ( streamReader->GetRemaining() < sizeof( streamHeader ) )
	{
		V6_ERROR( "Stream is too small to contain the stream header.\n" );
		return false;
	}

	streamReader->Read( sizeof( streamHeader ), &streamHeader );

	if ( memcmp( streamHeader.magic, CODEC_STREAM_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for stream header.\n", streamHeader.magic[0], streamHeader.magic[1], streamHeader.magic[2], streamHeader.magic[3] );
		return false;
	}

	if ( streamHeader.version != CODEC_STREAM_VERSION )
	{
		V6_ERROR( "Incompatible version %d for stream header.\n", streamHeader.version );
		return false;
	}

	const u32 frameOffsetSize = streamHeader.desc.sequenceCount * 4;
	const u32 sequenceByteOffsetSize = streamHeader.desc.sequenceCount * 4;

	if ( streamHeader.size != sizeof( streamHeader ) + frameOffsetSize + sequenceByteOffsetSize )
	{
		V6_ERROR( "Bad file size of %d bytes for stream header.\n", streamHeader.size );
		return false;
	}

	if ( (u32)streamReader->GetRemaining() < frameOffsetSize + sequenceByteOffsetSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for stream header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &streamHeader.desc, sizeof( streamHeader.desc ) );

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( frameOffsetSize + sequenceByteOffsetSize );

	data->frameOffsets = (u32*)buffer;
	streamReader->Read( frameOffsetSize, data->frameOffsets );

	data->sequenceByteOffsets = (u32*)(buffer + frameOffsetSize);
	streamReader->Read( sequenceByteOffsetSize, data->sequenceByteOffsets );

	V6_ASSERT( streamReader->GetPos() - beginPos == streamHeader.size );

	return buffer;
}

void* Codec_ReadSequence( IStreamReader* streamReader, CodecSequenceDesc_s* desc, CodecSequenceData_s* data, u32 sequenceID, IAllocator* allocator )
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

	if ( sequenceHeader.desc.sequenceID != sequenceID )
	{
		V6_ERROR( "Incompatible sequence ID %d for sequence desc.\n", sequenceHeader.desc.sequenceID );
		return nullptr;
	}

	u32 rangeDefSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		rangeDefSize += sequenceHeader.desc.rangeDefCounts[bucket] * sizeof( CodecRange_s );

	if ( sequenceHeader.size != sizeof( CodecSequenceHeader_s ) + rangeDefSize )
	{
		V6_ERROR( "Bad file size of %d bytes for sequence header.\n", sequenceHeader.size );
		return false;
	}

	if ( (u32)streamReader->GetRemaining() < rangeDefSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for sequence header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &sequenceHeader.desc, sizeof( sequenceHeader.desc ) );

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( rangeDefSize );

	data->rangeDefs = (CodecRange_s*)buffer;
	streamReader->Read( rangeDefSize, buffer );

	V6_ASSERT( streamReader->GetPos() - beginPos == sequenceHeader.size );

	return buffer;
}

void Codec_WriteStream( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc, const CodecStreamData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	const u32 frameOffsetSize = desc->sequenceCount * 4;
	const u32 sequenceByteOffsetSize = desc->sequenceCount * 4;

	CodecStreamHeader_s streamHeader = {};
	memcpy( streamHeader.magic, CODEC_STREAM_MAGIC, 4 );
	streamHeader.version = CODEC_STREAM_VERSION;
	streamHeader.size = sizeof( streamHeader ) + frameOffsetSize + sequenceByteOffsetSize;
	memcpy( &streamHeader.desc, desc, sizeof( streamHeader.desc ) );

	streamWriter->Write( &streamHeader, sizeof( streamHeader ) );
	streamWriter->Write( data->frameOffsets, frameOffsetSize );
	streamWriter->Write( data->sequenceByteOffsets, sequenceByteOffsetSize );

	V6_ASSERT( streamWriter->GetPos() - beginPos == streamHeader.size );
}

void Codec_WriteSequence( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc, const CodecSequenceData_s* data )
{
	const u32 beginPos = streamWriter->GetPos();

	u32 rangeDefSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		rangeDefSize += desc->rangeDefCounts[bucket] * sizeof( CodecRange_s );

	CodecSequenceHeader_s sequenceHeader = {};
	memcpy( sequenceHeader.magic, CODEC_SEQUENCE_MAGIC, 4 );
	sequenceHeader.version = CODEC_SEQUENCE_VERSION;
	sequenceHeader.size = sizeof( CodecSequenceHeader_s ) + rangeDefSize;
	memcpy( &sequenceHeader.desc, desc, sizeof( sequenceHeader.desc ) );

	streamWriter->Write( &sequenceHeader, sizeof( CodecSequenceHeader_s ) );
	streamWriter->Write( data->rangeDefs, rangeDefSize );
	
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

void* Codec_ReadFrame( IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack )
{
	ScopedStack scopedStack( stack );

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

	if ( frameHeader.desc.flags & CODEC_FRAME_FLAG_MOTION )
	{
		if ( frameHeader.desc.frameRank >= frameRank )
		{
			V6_ERROR( "Incompatible ref frame Rank %d for frame desc.\n", frameHeader.desc.frameRank );
			return nullptr;
		}

		memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );
		return (void*)1;
	}

	if ( frameHeader.desc.frameRank != frameRank )
	{
		V6_ERROR( "Incompatible frame ID %d for frame desc.\n", frameHeader.desc.frameRank );
		return nullptr;
	}

	u32 blockPosSize = 0;
#if CODEC_COLOR_COMPRESS == 1
	u32 blockDataCompressedSize = 0;
#endif // #if CODEC_COLOR_COMPRESS == 1
	u32 blockDataUncompressedSize = 0;
	u32 rangeIDSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockPosSize += frameHeader.desc.blockCounts[bucket] * 4;

		const u32 cellPerBucketCount = 1 << (2 + bucket);
#if CODEC_COLOR_COMPRESS == 1
		const u32 dataCompressedSize = cellPerBucketCount <= 32 ? sizeof( EncodedBlock_s ) : sizeof( EncodedBlockEx_s );
		blockDataCompressedSize += frameHeader.desc.blockCounts[bucket] * dataCompressedSize;
#endif // #if CODEC_COLOR_COMPRESS == 1
		const u32 dataUncompressedSize = cellPerBucketCount * 4;
		blockDataUncompressedSize += frameHeader.desc.blockCounts[bucket] * dataUncompressedSize;

		rangeIDSize += frameHeader.desc.blockRangeCounts[bucket] * 2;
	}

#if CODEC_COLOR_COMPRESS == 0
	u32 blockDataCompressedSize = blockDataUncompressedSize;
#endif

	const u32 compressedChunkSize = frameHeader.size - sizeof( CodecFrameHeader_s );

	if ( (u32)streamReader->GetRemaining() < compressedChunkSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for frame header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );

	IStreamReader* chunkReader = nullptr;
	const u32 decompressedChunkSize = blockPosSize + blockDataCompressedSize + rangeIDSize;
#if CODEC_FRAME_COMPRESS == 1
	u8* chunkLZ4 = (u8*)stack->alloc( compressedChunkSize );
	streamReader->Read( compressedChunkSize, chunkLZ4 );
	u8* decompressedChunk = (u8*)stack->alloc( decompressedChunkSize );
	if ( LZ4_decompress_fast( (char*)chunkLZ4, (char*)decompressedChunk, decompressedChunkSize ) != compressedChunkSize )
	{
		V6_ERROR( "LZ4 decompression failed.\n" );
		return nullptr;
	}
	CBufferReader decompressedChunkReader( decompressedChunk, decompressedChunkSize );
	chunkReader = &decompressedChunkReader;
#else
	V6_ASSERT( compressedChunkSize == decompressedChunkSize );
	chunkReader = streamReader;
#endif

	const u32 bufferSize = blockPosSize + blockDataUncompressedSize + rangeIDSize;

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( bufferSize );
	u8* chunk = buffer;

	chunkReader->Read( blockPosSize, chunk );
	data->blockPos = (u32*)chunk;
	chunk += blockPosSize;

#if CODEC_COLOR_COMPRESS == 1
	{
		u8* encodedData = (u8*)stack->alloc( blockDataCompressedSize );
		chunkReader->Read( blockDataCompressedSize, encodedData );

		u32* blockData = (u32*)chunk;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			const u32 blockCount = frameHeader.desc.blockCounts[bucket];
			const u32 cellPerBucketCount = 1 << (2 + bucket);
			const u32 dataCompressedSize = cellPerBucketCount <= 32 ? sizeof( EncodedBlock_s ) : sizeof( EncodedBlockEx_s );
			for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
			{
				const EncodedBlockEx_s* encodedBlock = (EncodedBlockEx_s*)encodedData;
				u32 cellCount = 0;
				Block_Decode( blockData, &cellCount, encodedBlock );
				for ( u32 cellID = cellCount; cellID < cellPerBucketCount; ++cellID )
					blockData[cellID] = 0xFFFFFFFF;
				encodedData += dataCompressedSize;
				blockData += cellPerBucketCount;
			}
		}

		V6_ASSERT( chunk + blockDataUncompressedSize == (u8*)blockData );
	}
#else
	chunkReader->Read( blockDataSize, chunk );
#endif // #if CODEC_COLOR_COMPRESS == 1
	data->blockData = (u32*)chunk;
	chunk += blockDataUncompressedSize;
	
	chunkReader->Read( rangeIDSize, chunk );
	data->rangeIDs = (u16*)chunk;
	chunk += rangeIDSize;
	
	V6_ASSERT( streamReader->GetPos() - beginPos == frameHeader.size );
	V6_ASSERT( (u32)(chunk - buffer) == bufferSize );

	return buffer;
}

bool Codec_WriteFrame( IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, IStack* stack )
{
	const u32 beginPos = streamWriter->GetPos();

	if ( desc->flags & CODEC_FRAME_FLAG_MOTION )
	{
		V6_ASSERT( data == nullptr );
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			V6_ASSERT( desc->blockCounts[bucket] == 0 );
			V6_ASSERT( desc->blockRangeCounts[bucket] == 0 );
		}

		CodecFrameHeader_s frameHeader = {};
		memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
		frameHeader.version = CODEC_FRAME_VERSION;
		frameHeader.size = sizeof( CodecFrameHeader_s );
		memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );
		streamWriter->Write( &frameHeader, sizeof( CodecFrameHeader_s ) );

		V6_ASSERT( streamWriter->GetPos() - beginPos == frameHeader.size );

		return true;
	}
	
	V6_ASSERT( data != nullptr );

	ScopedStack scopedStack( stack );

	u32 blockPosSize = 0;
#if CODEC_COLOR_COMPRESS == 1
	u32 blockDataCompressedSize = 0;
#endif
	u32 blockDataUncompressedSize = 0;
	u32 rangeIDSize = 0;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;

		const u32 cellPerBucketCount = 1 << (2 + bucket);
#if CODEC_COLOR_COMPRESS == 1
		const u32 dataCompressedSize = cellPerBucketCount <= 32 ? sizeof( EncodedBlock_s ) : sizeof( EncodedBlockEx_s );
#endif
		const u32 dataUncompressedSize = cellPerBucketCount * 4;

#if CODEC_COLOR_COMPRESS == 1
		blockDataCompressedSize += desc->blockCounts[bucket] * dataCompressedSize;
#endif
		blockDataUncompressedSize += desc->blockCounts[bucket] * dataUncompressedSize;

		V6_ASSERT( desc->blockRangeCounts[bucket] <= CODEC_RANGE_MAX_COUNT )
		rangeIDSize += desc->blockRangeCounts[bucket] * 2;
	}

#if CODEC_COLOR_COMPRESS == 1
	u8* blockCompressedData = (u8*)stack->alloc( blockDataCompressedSize );
	u8* encodedData = blockCompressedData;

	u32* uncompressedBlockData = data->blockData;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const u32 blockCount = desc->blockCounts[bucket];
		const u32 cellPerBucketCount = 1 << (2 + bucket);
		const u32 dataCompressedSize = cellPerBucketCount <= 32 ? sizeof( EncodedBlock_s ) : sizeof( EncodedBlockEx_s );
		for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
		{
			EncodedBlockEx_s* encodedBlock = (EncodedBlockEx_s*)encodedData;
			u32 cellCount = 0;
			Block_Encode( encodedBlock, uncompressedBlockData, cellPerBucketCount );
			encodedData += dataCompressedSize;
			uncompressedBlockData += cellPerBucketCount;
		}
	}

	V6_ASSERT( (u8*)data->blockData + blockDataUncompressedSize == (u8*)uncompressedBlockData );
#else
	u32 blockDataCompressedSize = blockDataUncompressedSize;
	u8* blockCompressedData = (u8*)data->blockData;
#endif // #if CODEC_COLOR_COMPRESS == 1

	const u32 chunkSize = blockPosSize + blockDataCompressedSize + rangeIDSize;

#if CODEC_FRAME_COMPRESS == 1
	const u32 chunkLZ4MaxSize = LZ4_compressBound( chunkSize );
	u8* chunckLZ4 = (u8*)stack->alloc( chunkLZ4MaxSize );

	CBufferWriter chunkWriter( stack->alloc( chunkSize ), chunkSize );
	chunkWriter.Write( data->blockPos, blockPosSize );
	chunkWriter.Write( blockCompressedData, blockDataCompressedSize );
	chunkWriter.Write( data->rangeIDs, rangeIDSize );

	const u32 chunkLZ4Size = LZ4_compress_HC( (char*)chunkWriter.GetBuffer(), (char*)chunckLZ4, chunkSize, chunkLZ4MaxSize, CODEC_LZ4_COMPRESSION_LEVEL );
	if ( chunkLZ4Size == 0 )
	{
		V6_ERROR( "LZ4 compression failed.\n" );
		return false;
	}
	// V6_MSG( "LZ4 compression: %5.2f%%.\n", chunkLZ4Size * 100.0f / chunkSize );
#endif

	CodecFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
	frameHeader.version = CODEC_FRAME_VERSION;
#if CODEC_FRAME_COMPRESS == 1
	frameHeader.size = sizeof( CodecFrameHeader_s ) + chunkLZ4Size;
#else
	frameHeader.size = sizeof( CodecFrameHeader_s ) + chunkSize;
#endif
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, sizeof( CodecFrameHeader_s ) );

#if CODEC_FRAME_COMPRESS == 1
	streamWriter->Write( chunckLZ4, chunkLZ4Size );
#else
	streamWriter->Write( data->blockPos, blockPosSize );
	streamWriter->Write( blockCompressedData, blockDataCompressedSize );
	streamWriter->Write( data->rangeIDs, rangeIDSize );
#endif

	V6_ASSERT( streamWriter->GetPos() - beginPos == frameHeader.size );

	return true;
}

END_V6_NAMESPACE
