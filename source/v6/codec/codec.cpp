/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#if CODEC_FRAME_COMPRESS == 1
#include <lz4/lib/lz4.h>
#include <lz4/lib/lz4hc.h>
#endif // #if CODEC_FRAME_COMPRESS == 1

#define CODEC_CLUSTER_SIZE				4096
#define CODEC_LZ4_COMPRESSION_LEVEL		4

BEGIN_V6_NAMESPACE

struct CodecStreamHeader_s
{
	char					magic[4];
	u32						version;
	u64						size;
	CodecStreamDesc_s		desc;
};

struct CodecSequenceHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecSequenceDesc_s		desc;
};

struct __CodecRawFrameHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecRawFrameDesc_s		desc;
};

V6_ALIGN( CODEC_CLUSTER_SIZE ) struct CodecRawFrameHeader_s : __CodecRawFrameHeader_s
{
	char					pad[CODEC_CLUSTER_SIZE - sizeof(__CodecRawFrameHeader_s) ];
};

struct CodecFrameHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecFrameDesc_s		desc;
};

static bool Codec_IsAlignedToClusterSize( u32 size )
{
	return (size & (CODEC_CLUSTER_SIZE-1)) == 0;
}

static bool Codec_IsAlignedToClusterSize( u64 size )
{
	return (size & (CODEC_CLUSTER_SIZE-1)) == 0;
}

static void Codec_AlignedWrite( IStreamWriter* streamWriter, const void* data, u64 size )
{
	V6_ASSERT( Codec_IsAlignedToClusterSize( ToU64( streamWriter->GetPos() ) ) );
	V6_ASSERT( Codec_IsAlignedToClusterSize( (u64)data ) );
	V6_ASSERT( Codec_IsAlignedToClusterSize( size ) );
	streamWriter->Write( data, ToX64( size ) );
}

static void Codec_AlignedRead( IStreamReader* streamReader, u64 size, void* data )
{
	V6_ASSERT( Codec_IsAlignedToClusterSize( ToU64( streamReader->GetPos() ) ) );
	V6_ASSERT( Codec_IsAlignedToClusterSize( (u64)data ) );
	V6_ASSERT( Codec_IsAlignedToClusterSize( size ) );
	streamReader->Read( ToX64( size ), data );
}

u32 Codec_GetClusterSize()
{
	return CODEC_CLUSTER_SIZE;
}

u32 Codec_AlignToClusterSize( u32 size )
{
	return (size + CODEC_CLUSTER_SIZE - 1) & ~(CODEC_CLUSTER_SIZE - 1);
}

u64 Codec_AlignToClusterSize( u64 size )
{
	return (size + CODEC_CLUSTER_SIZE - 1) & ~(CODEC_CLUSTER_SIZE - 1);
}

void* Codec_AlignToClusterSize( void* p )
{
	return (void*)(((uintptr_t)p + CODEC_CLUSTER_SIZE - 1) & ~(CODEC_CLUSTER_SIZE - 1));
}

void* Codec_AllocToClusterSizeAndFillPaddingWithZero( void** buffer, u64 size, IAllocator* allocator )
{
	const u64 allocSize = Codec_AlignToClusterSize( size ) + CODEC_CLUSTER_SIZE;
	
	void* rawData = allocator->alloc( allocSize );
	void* alignedData = Codec_AlignToClusterSize( rawData );
	const u64 pad0 = (u32)((uintptr_t)alignedData - (uintptr_t)rawData);
	memset( rawData, 0, pad0 );
	const u64 pad1 = allocSize - size - pad0;
	memset( (u8*)alignedData + size, 0, pad1 );

	if ( buffer )
		*buffer = rawData;

	return alignedData;
}

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

bool Codec_ReadStreamDesc( IStreamReader* streamReader, CodecStreamDesc_s* desc )
{
	const u64 beginPos = ToU64( streamReader->GetPos() );

	CodecStreamHeader_s streamHeader = {};

	if ( ToU64( streamReader->GetRemaining() ) < sizeof( streamHeader ) )
	{
		V6_ERROR( "Stream is too small to contain the stream header.\n" );
		return false;
	}

	streamReader->Read( ToX64( sizeof( streamHeader ) ), &streamHeader );

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

	if ( streamHeader.size != sizeof( streamHeader ) )
	{
		V6_ERROR( "Bad file size of %d bytes for stream header.\n", streamHeader.size );
		return false;
	}

	memcpy( desc, &streamHeader.desc, sizeof( streamHeader.desc ) );

	V6_ASSERT( ToU64( streamReader->GetPos() ) - beginPos == streamHeader.size );

	return true;
}

void* Codec_ReadSequence( IStreamReader* streamReader, CodecSequenceDesc_s* desc, CodecSequenceData_s* data, u32 sequenceID, IAllocator* allocator )
{	
	const u64 beginPos = ToU64( streamReader->GetPos() );

	if ( ToU64( streamReader->GetRemaining() ) < sizeof( CodecSequenceHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the sequence header.\n" );
		return false;
	}

	CodecSequenceHeader_s sequenceHeader = {};
	streamReader->Read( ToX64( sizeof( CodecSequenceHeader_s ) ), &sequenceHeader );

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

	const u64 rangeDefSize = sequenceHeader.desc.rangeDefCount * sizeof( CodecRange_s );

	if ( sequenceHeader.size != sizeof( CodecSequenceHeader_s ) + rangeDefSize )
	{
		V6_ERROR( "Bad file size of %d bytes for sequence header.\n", sequenceHeader.size );
		return false;
	}

	if ( ToU64( streamReader->GetRemaining() ) < rangeDefSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for sequence header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &sequenceHeader.desc, sizeof( sequenceHeader.desc ) );

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( rangeDefSize );

	data->rangeDefs = (CodecRange_s*)buffer;
	streamReader->Read( ToX64( rangeDefSize ), buffer );

	V6_ASSERT( ToU64( streamReader->GetPos() ) - beginPos == sequenceHeader.size );

	return buffer;
}

void Codec_WriteStreamDesc( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );

	CodecStreamHeader_s streamHeader = {};
	memcpy( streamHeader.magic, CODEC_STREAM_MAGIC, 4 );
	streamHeader.version = CODEC_STREAM_VERSION;
	streamHeader.size = sizeof( streamHeader );
	memcpy( &streamHeader.desc, desc, sizeof( streamHeader.desc ) );

	streamWriter->Write( &streamHeader, ToX64( sizeof( streamHeader ) ) );

	V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == streamHeader.size );
}

void Codec_WriteSequence( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc, const CodecSequenceData_s* data )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );

	const u64 rangeDefSize = desc->rangeDefCount * sizeof( CodecRange_s );

	CodecSequenceHeader_s sequenceHeader = {};
	memcpy( sequenceHeader.magic, CODEC_SEQUENCE_MAGIC, 4 );
	sequenceHeader.version = CODEC_SEQUENCE_VERSION;
	sequenceHeader.size = (u32)(sizeof( CodecSequenceHeader_s ) + rangeDefSize);
	memcpy( &sequenceHeader.desc, desc, sizeof( sequenceHeader.desc ) );

	streamWriter->Write( &sequenceHeader, ToX64( sizeof( CodecSequenceHeader_s ) ) );
	streamWriter->Write( data->rangeDefs, ToX64( rangeDefSize ) );
	
	V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == sequenceHeader.size );
}

void Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );

	u64 blockPosSize = 0;
	u64 blockDataSize = 0;

	u32 cellPerBucketCount = 4;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;
		blockDataSize += desc->blockCounts[bucket] * cellPerBucketCount * 4;
	}

	void* blockPosBuffer;
	void* blockPosAligned = Codec_AllocToClusterSizeAndFillPaddingWithZero( &blockPosBuffer, blockPosSize, allocator );
	memcpy( blockPosAligned, data->blockPos, blockPosSize );

	void* blockDataBuffer;
	void* blockDataAligned = Codec_AllocToClusterSizeAndFillPaddingWithZero( &blockDataBuffer, blockDataSize, allocator );
	memcpy( blockDataAligned, data->blockData, blockDataSize );

	if ( buffer )
	{
		buffer->blockPosBuffer = blockPosBuffer;
		buffer->blockDataBuffer = blockDataBuffer;
	}

	const u64 blockPosAlignedSize = Codec_AlignToClusterSize( blockPosSize );
	const u64 blockDataAlignedSize = Codec_AlignToClusterSize( blockDataSize );

	CodecRawFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_RAWFRAME_MAGIC, 4 );
	frameHeader.version = CODEC_RAWFRAME_VERSION;
	frameHeader.size = (u32)(sizeof( CodecRawFrameHeader_s ) + blockPosAlignedSize + blockDataAlignedSize);
	V6_ASSERT( Codec_IsAlignedToClusterSize( frameHeader.size ) );
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	Codec_AlignedWrite( streamWriter, &frameHeader, sizeof( CodecRawFrameHeader_s ) );
	Codec_AlignedWrite( streamWriter, blockPosAligned, blockPosAlignedSize );
	Codec_AlignedWrite( streamWriter, blockDataAligned, blockDataAlignedSize );

	V6_ASSERT( ToU64( streamWriter->GetPos() )- beginPos == frameHeader.size );
}

bool Codec_ReadRawFrameDesc( IStreamReader* streamReader, CodecRawFrameDesc_s* desc )
{
	if ( ToU64( streamReader->GetRemaining() ) < sizeof( CodecRawFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return false;
	}

	CodecRawFrameHeader_s frameHeader = {};
	Codec_AlignedRead( streamReader, sizeof( CodecRawFrameHeader_s ), &frameHeader );

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

	u64 blockPosSize = 0;
	u64 blockDataSize = 0;

	u32 cellPerBucketCount = 4;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += frameHeader.desc.blockCounts[bucket] * 4;
		blockDataSize += frameHeader.desc.blockCounts[bucket] * cellPerBucketCount * 4;
	}

	blockPosSize = Codec_AlignToClusterSize( blockPosSize );
	blockDataSize = Codec_AlignToClusterSize( blockDataSize );

	if ( frameHeader.size != sizeof( CodecRawFrameHeader_s ) + blockPosSize + blockDataSize )
	{
		V6_ERROR( "Bad file size of %d bytes for frame header.\n", frameHeader.size );
		return false;
	}

	memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );
	
	if ( ToU64( streamReader->GetRemaining() ) < (int)blockPosSize )
	{
		V6_ERROR( "Stream is too small to contain the frame block pos.\n" );
		return false;
	}
	
	return true;
}

bool Codec_ReadRawFrame( IStreamReader* streamReader, CodecRawFrameDesc_s* desc, CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator )
{
	if ( !Codec_ReadRawFrameDesc( streamReader, desc ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return false;
	}

	u64 blockPosSize = 0;
	u64 blockDataSize = 0;

	u32 cellPerBucketCount = 4;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		blockPosSize += desc->blockCounts[bucket] * 4;
		blockDataSize += desc->blockCounts[bucket] * cellPerBucketCount * 4;
	}

	if ( blockPosSize == 0 )
	{
		data->blockPos = nullptr;
		data->blockData = nullptr;
		if ( buffer )
		{
			buffer->blockPosBuffer = nullptr;
			buffer->blockDataBuffer = nullptr;
		}
		V6_ASSERT( blockDataSize == 0 );
		return true;
	}

	blockPosSize = Codec_AlignToClusterSize( blockPosSize );
	blockDataSize = Codec_AlignToClusterSize( blockDataSize );

	void* blockPosBuffer;
	data->blockPos = Codec_AllocToClusterSizeAndFillPaddingWithZero( &blockPosBuffer, blockPosSize, allocator );

	if ( data->blockPos == nullptr )
	{
		V6_ERROR( "Not enough memory to allocate frame block pos of %d bytes.\n", blockPosSize );
		return false;
	}

	Codec_AlignedRead( streamReader, blockPosSize, data->blockPos );

	if ( ToU64( streamReader->GetRemaining() ) < (int)blockDataSize )
	{
		V6_ERROR( "Stream is too small to contain the frame block data.\n" );
		return false;
	}

	void* blockDataBuffer;
	data->blockData = Codec_AllocToClusterSizeAndFillPaddingWithZero( &blockDataBuffer, blockDataSize, allocator );

	if ( data->blockData == nullptr )
	{
		V6_ERROR( "Not enough memory to allocate frame block data of %d bytes.\n", blockDataSize );
		return false;
	}

	Codec_AlignedRead( streamReader, blockDataSize, data->blockData );

	if ( buffer )
	{
		buffer->blockPosBuffer = blockPosBuffer;
		buffer->blockDataBuffer = blockDataBuffer;
	}
	
	return true;
}

void* Codec_ReadFrame( IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack )
{
	ScopedStack scopedStack( stack );

	const u64 beginPos = ToU64( streamReader->GetPos() );

	if ( ToU64( streamReader->GetRemaining() ) < sizeof( CodecFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return nullptr;
	}

	CodecFrameHeader_s frameHeader = {};
	streamReader->Read( ToX64( sizeof( CodecFrameHeader_s ) ), &frameHeader );

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

	const u64 blockPosSize = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellPresence0Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellPresence1Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellEndColorSize = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex0Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex1Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex2Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex3Size = frameHeader.desc.blockCount * 4;
	const u64 rangeIDSize = frameHeader.desc.blockRangeCount * 2;

	const u64 compressedChunkSize = frameHeader.size - sizeof( CodecFrameHeader_s );

	if ( ToU64( streamReader->GetRemaining() ) < compressedChunkSize )
	{
		V6_ERROR( "Bad stream size of %d bytes for frame header.\n", streamReader->GetRemaining() );
		return nullptr;
	}

	memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );

	const u64 decompressedChunkSize = blockPosSize + blockDataCellPresence0Size + blockDataCellPresence1Size + blockDataCellEndColorSize + blockDataCellColorIndex0Size + blockDataCellColorIndex1Size + blockDataCellColorIndex2Size + blockDataCellColorIndex3Size + rangeIDSize;

	// todo: aligned alloc
	u8* buffer = (u8*)allocator->alloc( decompressedChunkSize );
	u8* chunk = buffer;

#if CODEC_FRAME_COMPRESS == 1
	{
		u8* chunkLZ4 = (u8*)stack->alloc( compressedChunkSize );
		streamReader->Read( ToX64( compressedChunkSize ), chunkLZ4 );
		if ( LZ4_decompress_fast( (char*)chunkLZ4, (char*)chunk, (int)decompressedChunkSize ) != compressedChunkSize )
		{
			V6_ERROR( "LZ4 decompression failed.\n" );
			return nullptr;
		}
	}
#else
	V6_ASSERT( compressedChunkSize == decompressedChunkSize );
	streamReader->Read( ToX64( decompressedChunkSize ), chunk );
#endif

	data->blockPos = (u32*)chunk;
	chunk += blockPosSize;

	data->blockCellPresences0 = (u32*)chunk;
	chunk += blockDataCellPresence0Size;

	data->blockCellPresences1 = (u32*)chunk;
	chunk += blockDataCellPresence1Size;

	data->blockCellEndColors = (u32*)chunk;
	chunk += blockDataCellEndColorSize;

	data->blockCellColorIndices0 = (u32*)chunk;
	chunk += blockDataCellColorIndex0Size;

	data->blockCellColorIndices1 = (u32*)chunk;
	chunk += blockDataCellColorIndex1Size;

	data->blockCellColorIndices2 = (u32*)chunk;
	chunk += blockDataCellColorIndex2Size;

	data->blockCellColorIndices3 = (u32*)chunk;
	chunk += blockDataCellColorIndex3Size;
	
	data->rangeIDs = (u16*)chunk;
	chunk += rangeIDSize;
	
	V6_ASSERT( ToU64( streamReader->GetPos() ) - beginPos == frameHeader.size );
	V6_ASSERT( chunk - buffer == decompressedChunkSize );

	return buffer;
}

bool Codec_WriteFrame( IStreamWriter* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, IStack* stack )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );

	if ( desc->flags & CODEC_FRAME_FLAG_MOTION )
	{
		V6_ASSERT( data == nullptr );
		V6_ASSERT( desc->blockCount == 0 );
		V6_ASSERT( desc->blockRangeCount == 0 );

		CodecFrameHeader_s frameHeader = {};
		memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
		frameHeader.version = CODEC_FRAME_VERSION;
		frameHeader.size = sizeof( CodecFrameHeader_s );
		memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );
		streamWriter->Write( &frameHeader, ToX64( sizeof( CodecFrameHeader_s ) ) );

		V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == frameHeader.size );

		return true;
	}
	
	V6_ASSERT( data != nullptr );

	ScopedStack scopedStack( stack );

	const u64 blockPosSize = desc->blockCount * 4;
	const u64 blockDataCellPresence0Size = desc->blockCount * 4;
	const u64 blockDataCellPresence1Size = desc->blockCount * 4;
	const u64 blockDataCellEndColorSize = desc->blockCount * 4;
	const u64 blockDataCellColorIndex0Size = desc->blockCount * 4;
	const u64 blockDataCellColorIndex1Size = desc->blockCount * 4;
	const u64 blockDataCellColorIndex2Size = desc->blockCount * 4;
	const u64 blockDataCellColorIndex3Size = desc->blockCount * 4;
	V6_ASSERT( desc->blockRangeCount <= CODEC_RANGE_MAX_COUNT );
	const u64 rangeIDSize = desc->blockRangeCount * 2;

	const u64 chunkSize = blockPosSize + blockDataCellPresence0Size + blockDataCellPresence1Size + blockDataCellEndColorSize + blockDataCellColorIndex0Size + blockDataCellColorIndex1Size + blockDataCellColorIndex2Size + blockDataCellColorIndex3Size + rangeIDSize;

#if CODEC_FRAME_COMPRESS == 1
	const u64 chunkLZ4MaxSize = LZ4_compressBound( (int)chunkSize );
	u8* chunckLZ4 = (u8*)stack->alloc( chunkLZ4MaxSize );

	CBufferWriter chunkWriter( stack->alloc( chunkSize ), ToX64( chunkSize ) );
	chunkWriter.Write( data->blockPos, ToX64( blockPosSize ) );
	chunkWriter.Write( data->blockCellPresences0, ToX64( blockDataCellPresence0Size ) );
	chunkWriter.Write( data->blockCellPresences1, ToX64( blockDataCellPresence1Size ) );
	chunkWriter.Write( data->blockCellEndColors, ToX64( blockDataCellEndColorSize ) );
	chunkWriter.Write( data->blockCellColorIndices0, ToX64( blockDataCellColorIndex0Size ) );
	chunkWriter.Write( data->blockCellColorIndices1, ToX64( blockDataCellColorIndex1Size ) );
	chunkWriter.Write( data->blockCellColorIndices2, ToX64( blockDataCellColorIndex2Size ) );
	chunkWriter.Write( data->blockCellColorIndices3, ToX64( blockDataCellColorIndex3Size ) );
	chunkWriter.Write( data->rangeIDs, ToX64( rangeIDSize ) );

	const u64 chunkLZ4Size = LZ4_compress_HC( (char*)chunkWriter.GetBuffer(), (char*)chunckLZ4, (int)chunkSize, (int)chunkLZ4MaxSize, CODEC_LZ4_COMPRESSION_LEVEL );
	if ( chunkLZ4Size == 0 )
	{
		V6_ERROR( "LZ4 compression failed.\n" );
		return false;
	}
	// V6_MSG( "LZ4 compression: %5.2f\%.\n", chunkLZ4Size * 100.0f / chunkSize );
#endif

	CodecFrameHeader_s frameHeader = {};
	memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
	frameHeader.version = CODEC_FRAME_VERSION;
#if CODEC_FRAME_COMPRESS == 1
	frameHeader.size = (u32)(sizeof( CodecFrameHeader_s ) + chunkLZ4Size);
#else
	frameHeader.size = (u32)(sizeof( CodecFrameHeader_s ) + chunkSize);
#endif
	memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );

	streamWriter->Write( &frameHeader, ToX64( sizeof( CodecFrameHeader_s ) ) );

#if CODEC_FRAME_COMPRESS == 1
	streamWriter->Write( chunckLZ4, ToX64( chunkLZ4Size ) );
#else
	streamWriter->Write( data->blockPos, ToX64( blockPosSize ) );
	streamWriter->Write( data->blockCellPresences0, ToX64( blockDataCellPresence0Size ) );
	streamWriter->Write( data->blockCellPresences1, ToX64( blockDataCellPresence1Size ) );
	streamWriter->Write( data->blockCellEndColors, ToX64( blockDataCellEndColorSize ) );
	streamWriter->Write( data->blockCellColorIndices0, ToX64( blockDataCellColorIndex0Size ) );
	streamWriter->Write( data->blockCellColorIndices1, ToX64( blockDataCellColorIndex1Size ) );
	streamWriter->Write( data->blockCellColorIndices2, ToX64( blockDataCellColorIndex2Size ) );
	streamWriter->Write( data->blockCellColorIndices3, ToX64( blockDataCellColorIndex3Size ) );
	streamWriter->Write( data->rangeIDs, ToX64( rangeIDSize ) );
#endif

	V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == frameHeader.size );

	return true;
}

END_V6_NAMESPACE
