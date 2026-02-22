/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/core/bit.h>
#include <v6/core/memory.h>
#include <v6/core/random.h>
#include <v6/core/stream.h>
#if CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_LZ4
#include <lz4/lib/lz4.h>
#include <lz4/lib/lz4hc.h>
#elif CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_ZSTD // #if CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_LZ4
#include <zstd/lib/zstd.h>
#endif // #if CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_ZSTD

#define CODEC_LZ4_COMPRESSION_LEVEL		4
#define CODEC_ZSTD_COMPRESSION_LEVEL	19

#define CODEC_PRESENCE_STATS 0
#define CODEC_ZSTD_STATS     0

BEGIN_V6_NAMESPACE

struct CodecStreamHeader_s
{
	char					magic[4];
	u32						version;
	u64						size;
	CodecStreamDesc_s		desc;
};

struct __CodecSequenceHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	CodecSequenceDesc_s		desc;
};

V6_ALIGN( CODEC_CLUSTER_SIZE ) struct CodecSequenceHeader_s : __CodecSequenceHeader_s
{
	char					pad[CODEC_CLUSTER_SIZE - sizeof( __CodecSequenceHeader_s ) ];
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
	char					pad[CODEC_CLUSTER_SIZE - sizeof( __CodecRawFrameHeader_s ) ];
};

V6_ALIGN( CODEC_BUFFER_ALIGNMENT ) struct CodecFrameHeader_s
{
	char					magic[4];
	u32						version;
	u32						size;
	u32						uncompressedDataAlignedSize;
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

template < typename T >
static bool Codec_IsAlignedToBufferSize( T size )
{
	return (size & (CODEC_BUFFER_ALIGNMENT-1)) == 0;
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

template < typename T >
V6_INLINE T Codec_AlignToBufferSize( T size )
{ 
	return PowOfTwoRoundUp< CODEC_BUFFER_ALIGNMENT >( size );
}

void* Codec_AllocToClusterSizeAndFillPaddingWithZero( void** buffer, u64 size, IAllocator* allocator )
{
	const u64 allocSize = Codec_AlignToClusterSize( size ) + CODEC_CLUSTER_SIZE;
	
	void* rawData = allocator->alloc( allocSize, "CodecRawData" );
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

void* Codec_ReadStreamDesc( IStreamReader* streamReader, CodecStreamDesc_s* desc, CodecStreamData_s* data, IAllocator* allocator )
{
	const u64 beginPos = ToU64( streamReader->GetPos() );
	V6_ASSERT( Codec_IsAlignedToClusterSize( beginPos ) );

	if ( Codec_Error() || ToU64( streamReader->GetRemaining() ) < CODEC_STREAM_HEADER_SIZE )
	{
		V6_ERROR( "Stream is too small to contain the stream header.\n" );
		return nullptr;
	}

	V6_ASSERT( sizeof( CodecStreamHeader_s ) <= CODEC_CLUSTER_SIZE );

	V6_ALIGN( CODEC_CLUSTER_SIZE ) u8 bufferStack[CODEC_CLUSTER_SIZE];
	void* buffer = nullptr;
	u8* chunkBegin;
	
	if ( data )
	{
		chunkBegin = (u8*)Codec_AllocToClusterSizeAndFillPaddingWithZero( &buffer, CODEC_STREAM_HEADER_SIZE, allocator );
		Codec_AlignedRead( streamReader, CODEC_STREAM_HEADER_SIZE, chunkBegin );
	}
	else
	{
		chunkBegin = bufferStack;
		Codec_AlignedRead( streamReader, CODEC_CLUSTER_SIZE, chunkBegin );
	}

	u8* chunk = chunkBegin;

	CodecStreamHeader_s streamHeader = {};

	memcpy( &streamHeader, chunk, sizeof( streamHeader ) );
	chunk += sizeof( streamHeader );

	if ( Codec_Error() || memcmp( streamHeader.magic, CODEC_STREAM_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for stream header.\n", streamHeader.magic[0], streamHeader.magic[1], streamHeader.magic[2], streamHeader.magic[3] );
		goto clean_up;
	}

	if ( Codec_Error() || (streamHeader.version != CODEC_STREAM_VERSION) )
	{
		V6_ERROR( "Incompatible version %d for stream header.\n", streamHeader.version );
		goto clean_up;
	}

	if ( Codec_Error() || (streamHeader.size != CODEC_STREAM_HEADER_SIZE) )
	{
		V6_ERROR( "Bad file size of %d bytes for stream header.\n", streamHeader.size );
		goto clean_up;
	}

	V6_ASSERT( ToU64( streamReader->GetPos() ) - beginPos == streamHeader.size );

	memcpy( desc, &streamHeader.desc, sizeof( streamHeader.desc ) );
	
	if ( !data )
		return (void*)1;

	const u32 sequenceInfoSize = desc->sequenceCount * sizeof( CodecSequenceInfo_s );

	u32 keySize = 0;
	u32 valueSize = 0;
	for ( u32 keyID = 0; keyID < desc->keyCount; ++keyID )
	{
		keySize += desc->keySizes[keyID];
		valueSize += desc->valueSizes[keyID];
	}

	const u32 bufferSize = sizeof( CodecStreamHeader_s ) + sequenceInfoSize + keySize + valueSize;

	if ( Codec_Error() || bufferSize > CODEC_STREAM_HEADER_SIZE )
	{
		V6_ERROR( "Stream header size of %d KB is bigger than the limit of %d KB.\n", DivKB( bufferSize ), DivKB( CODEC_STREAM_HEADER_SIZE ) );
		goto clean_up;
	}

	data->sequenceInfos = (CodecSequenceInfo_s*)chunk;
	chunk += sequenceInfoSize;

	data->keys = (char*)chunk;
	chunk += keySize;
	
	data->values = chunk;
	chunk += valueSize;

	V6_ASSERT( chunk - chunkBegin == bufferSize );

	return buffer;

clean_up:
	allocator->free( buffer );
	return nullptr;
}

bool Codec_ReadSequenceDesc( IStreamReader* streamReader, CodecSequenceDesc_s* desc )
{	
	const u64 beginPos = ToU64( streamReader->GetPos() );
	V6_ASSERT( Codec_IsAlignedToClusterSize( beginPos ) );

	if ( Codec_Error() || ToU64( streamReader->GetRemaining() ) < sizeof( CodecSequenceHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the sequence header.\n" );
		return false;
	}

	CodecSequenceHeader_s sequenceHeader = {};
	Codec_AlignedRead( streamReader, sizeof( CodecSequenceHeader_s ), &sequenceHeader );

	if ( Codec_Error() || memcmp( sequenceHeader.magic, CODEC_SEQUENCE_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for sequence header.\n", sequenceHeader.magic[0], sequenceHeader.magic[1], sequenceHeader.magic[2], sequenceHeader.magic[3] );
		return false;
	}

	if ( Codec_Error() ||sequenceHeader.version != CODEC_SEQUENCE_VERSION )
	{
		V6_ERROR( "Incompatible version %d for sequence header.\n", sequenceHeader.version );
		return false;
	}

	if ( Codec_Error() ||sequenceHeader.size != sizeof( CodecSequenceHeader_s ) )
	{
		V6_ERROR( "Bad file size of %d bytes for sequence header.\n", sequenceHeader.size );
		return false;
	}

	memcpy( desc, &sequenceHeader.desc, sizeof( sequenceHeader.desc ) );

	V6_ASSERT( ToU64( streamReader->GetPos() ) - beginPos == sequenceHeader.size );

	return true;
}

bool Codec_WriteStreamDesc( IStreamWriter* streamWriter, const CodecStreamDesc_s* desc, const CodecStreamData_s* data, IStack* stack )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );
	V6_ASSERT( Codec_IsAlignedToClusterSize( beginPos ) );

	const u32 sequenceInfoSize = desc->sequenceCount * sizeof( CodecSequenceInfo_s );

	u32 keySize = 0;
	u32 valueSize = 0;
	for ( u32 keyID = 0; keyID < desc->keyCount; ++keyID )
	{
		keySize += desc->keySizes[keyID];
		valueSize += desc->valueSizes[keyID];
	}

	const u32 bufferSize = sizeof( CodecStreamHeader_s ) + sequenceInfoSize + keySize + valueSize;

	if ( bufferSize > CODEC_STREAM_HEADER_SIZE )
	{
		V6_ERROR( "Stream header size of %d KB is bigger than the limit of %d KB.\n", DivKB( bufferSize ), DivKB( CODEC_STREAM_HEADER_SIZE ) );
		return false;
	}

	CodecStreamHeader_s streamHeader = {};
	memcpy( streamHeader.magic, CODEC_STREAM_MAGIC, 4 );
	streamHeader.version = CODEC_STREAM_VERSION;
	streamHeader.size = CODEC_STREAM_HEADER_SIZE;
	V6_ASSERT( Codec_IsAlignedToClusterSize( streamHeader.size ) );
	memcpy( &streamHeader.desc, desc, sizeof( streamHeader.desc ) );

	ScopedStack scopedStack( stack );

	CBufferWriter chunkWriter( Codec_AllocToClusterSizeAndFillPaddingWithZero( nullptr, CODEC_STREAM_HEADER_SIZE, stack ), ToX64( CODEC_STREAM_HEADER_SIZE ) );

	chunkWriter.Write( &streamHeader, ToX64( sizeof( CodecStreamHeader_s ) ) );
	if ( data )
	{
		chunkWriter.Write( data->sequenceInfos, ToX64( sequenceInfoSize ) );
		chunkWriter.Write( data->keys, ToX64( keySize ) );
		chunkWriter.Write( data->values, ToX64( valueSize ) );
	}

	V6_ASSERT( ToU64( chunkWriter.GetPos() ) == bufferSize );

	chunkWriter.WriteZero( CODEC_STREAM_HEADER_SIZE - bufferSize );

	V6_ASSERT( ToU64( chunkWriter.GetPos() ) == CODEC_STREAM_HEADER_SIZE );

	Codec_AlignedWrite( streamWriter, chunkWriter.GetBuffer(), CODEC_STREAM_HEADER_SIZE );

	V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == streamHeader.size );

	return true;
}

void Codec_WriteSequenceDesc( IStreamWriter* streamWriter, const CodecSequenceDesc_s* desc )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );
	V6_ASSERT( Codec_IsAlignedToClusterSize( beginPos ) );

	CodecSequenceHeader_s sequenceHeader = {};
	memcpy( sequenceHeader.magic, CODEC_SEQUENCE_MAGIC, 4 );
	sequenceHeader.version = CODEC_SEQUENCE_VERSION;
	sequenceHeader.size = (u32)sizeof( CodecSequenceHeader_s );
	V6_ASSERT( Codec_IsAlignedToClusterSize( sequenceHeader.size ) );
	memcpy( &sequenceHeader.desc, desc, sizeof( sequenceHeader.desc ) );

	Codec_AlignedWrite( streamWriter, &sequenceHeader, sizeof( CodecSequenceHeader_s ) );
	
	V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == sequenceHeader.size );
}

void Codec_WriteRawFrame( IStreamWriter* streamWriter, const CodecRawFrameDesc_s* desc, const CodecRawFrameData_s* data, CodecRawFrameBuffer_s* buffer, IAllocator* allocator )
{
	const u64 beginPos = ToU64( streamWriter->GetPos() );
	V6_ASSERT( Codec_IsAlignedToClusterSize( beginPos ) );

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

u32 Codec_ReadFrameSizeOnly( IStreamReader* streamReader )
{
	if ( Codec_Error() || ToU64( streamReader->GetRemaining() ) < sizeof( CodecFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return 0;
	}

	CodecFrameHeader_s frameHeader;
	streamReader->Read( ToX64( sizeof( frameHeader ) ), &frameHeader );
	V6_ASSERT( frameHeader.size >= sizeof( frameHeader ) );
	streamReader->Skip( ToX64( frameHeader.size - sizeof( frameHeader ) ) );
	
	return frameHeader.size;
}

void Codec_LogCellPresence(u32 const *blockCellPresences0, u32 const *blockCellPresences1, u32 blockCount) {
#if CODEC_PRESENCE_STATS
  u32 cellCount = 0;
  for (u32 blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
    cellCount += Bit_GetBitHighCount(*blockCellPresences0) + Bit_GetBitHighCount(*blockCellPresences1);
    ++blockCellPresences0;
    ++blockCellPresences1;
  }

  V6_DEVMSG("\n *** %d Kblocks, %d Kcells, avg of %.1f cells/block\n", DivKB(blockCount), DivKB(cellCount), float(cellCount) / blockCount);
#endif
}

void Codec_LogCompressedSize( void *chunk, u64 chunkSize, const char* desc, IAllocator* allocator, IStack* stack) {
#if CODEC_ZSTD_STATS
  ScopedStack scopedStack( stack );

  const u64 chunkMaxSize = ZSTD_compressBound(chunkSize);
  u8* chunkCompressed = (u8*)stack->alloc( chunkMaxSize , "dataMaxSize" );
  const u64 chunkCompressedSize = ZSTD_compress( chunkCompressed, chunkMaxSize , chunk, chunkSize, CODEC_ZSTD_COMPRESSION_LEVEL );
  V6_DEVMSG("\n *** %s: uncompressed %d KB, compressed %d KB\n", desc, DivKB(chunkSize), DivKB(chunkCompressedSize));
#endif
}

u32 Codec_ReadFrame( void** buffer, IStreamReader* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack )
{
	*buffer = nullptr;

	const u64 beginPos = ToU64( streamReader->GetPos() );

	if ( Codec_Error() || ToU64( streamReader->GetRemaining() ) < sizeof( CodecFrameHeader_s ) )
	{
		V6_ERROR( "Stream is too small to contain the frame header.\n" );
		return 0;
	}

	CodecFrameHeader_s frameHeader;
	streamReader->Read( ToX64( sizeof( frameHeader ) ), &frameHeader );
	
	if ( Codec_Error() || memcmp( frameHeader.magic, CODEC_FRAME_MAGIC, 4 ) != 0 )
	{
		V6_ERROR( "Invalid magic '%c%c%c%c' for frame header.\n", frameHeader.magic[0], frameHeader.magic[1], frameHeader.magic[2], frameHeader.magic[3] );
		return 0;
	}

	if ( Codec_Error() ||frameHeader.version != CODEC_FRAME_VERSION )
	{
		V6_ERROR( "Incompatible version %d for frame header.\n", frameHeader.version );
		return 0;
	}

	if ( frameHeader.desc.flags & CODEC_FRAME_FLAG_MOTION )
	{
		if ( Codec_Error() || frameHeader.desc.frameRank >= frameRank )
		{
			V6_ERROR( "Incompatible ref frame Rank %d for frame desc.\n", frameHeader.desc.frameRank );
			return 0;
		}

		memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );
		*buffer = (void*)1;
		return 0;
	}

	if ( Codec_Error() || frameHeader.desc.frameRank != frameRank )
	{
		V6_ERROR( "Incompatible frame ID %d for frame desc.\n", frameHeader.desc.frameRank );
		return 0;
	}

	const u64 remainingSize = sizeof( CodecFrameHeader_s ) + ToU64( streamReader->GetRemaining() );
	if ( Codec_Error() || remainingSize < frameHeader.size )
	{
		V6_ERROR( "Bad stream size of %d bytes for frame header.\n", streamReader->GetRemaining() );
		return 0;
	}

	memcpy( desc, &frameHeader.desc, sizeof( frameHeader.desc ) );

	const u64 blockPosSize = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellPresence0Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellPresence1Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellEndColorSize = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex0Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex1Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex2Size = frameHeader.desc.blockCount * 4;
	const u64 blockDataCellColorIndex3Size = frameHeader.desc.blockCount * 4;
	const u64 blockRangeSize = frameHeader.desc.blockRangeCount * sizeof( CodecBlockRange_s );

	const u64 chunkUnpackedAlignedSize = 
		Codec_AlignToBufferSize( blockPosSize ) + 
		Codec_AlignToBufferSize( blockDataCellPresence0Size ) + 
		Codec_AlignToBufferSize( blockDataCellPresence1Size ) + 
		Codec_AlignToBufferSize( blockDataCellEndColorSize ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex0Size ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex1Size ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex2Size ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex3Size ) + 
		Codec_AlignToBufferSize( blockRangeSize );

	V6_ASSERT( chunkUnpackedAlignedSize >= frameHeader.uncompressedDataAlignedSize );

	ScopedStack scopedStack( stack );

#if CODEC_FRAME_COMPRESS != CODEC_FRAME_COMPRESS_TYPE_NONE
	const u64 compressedDataSize = frameHeader.size - sizeof( frameHeader );
	u8* const chunkCompressed = (u8*)stack->alloc( compressedDataSize, "CodecCompressedData" );
	streamReader->Read( ToX64( compressedDataSize ), chunkCompressed );

	const u64 packedAlignedOffsetChunk = chunkUnpackedAlignedSize - frameHeader.uncompressedDataAlignedSize;

	u8* const chunkBegin = (u8*)allocator->alloc_aligned< CODEC_BUFFER_ALIGNMENT >( buffer, chunkUnpackedAlignedSize, "CodecFrame" );
	u8* chunk = chunkBegin;

#if CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_LZ4
	{
		if ( Codec_Error() || LZ4_decompress_fast( (char*)chunkCompressed, (char*)(chunk + packedAlignedOffsetChunk), (int)frameHeader.uncompressedDataAlignedSize ) != compressedDataSize )
		{
			V6_ERROR( "LZ4 decompression failed.\n" );
			allocator->free( *buffer )
			*buffer = nullptr;
			return 0;
		}
	}
#elif CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_ZSTD
	{
		const u64 chunkZSTDSize = ZSTD_decompress( chunk + packedAlignedOffsetChunk, frameHeader.uncompressedDataAlignedSize, chunkCompressed, compressedDataSize );
		if ( Codec_Error() || chunkZSTDSize != frameHeader.uncompressedDataAlignedSize )
		{
			if ( ZSTD_isError( chunkZSTDSize ) )
				V6_ERROR( "ZSTD decompression failed: %s\n", ZSTD_getErrorName( chunkZSTDSize ) );
			else
				V6_ERROR( "ZSTD decompression failed: %lld != %lld\n", chunkZSTDSize, frameHeader.uncompressedDataAlignedSize );
			allocator->free( *buffer );
			*buffer = nullptr;
			return 0;
		}
	}
#endif
#else // #if CODEC_FRAME_COMPRESS != CODEC_FRAME_COMPRESS_TYPE_NONE
	V6_ASSERT( frameHeader.size - sizeof( frameHeader ) == chunkUnpackedAlignedSize );

	u8* const chunkBegin = (u8*)stack->alloc_aligned< CODEC_BUFFER_ALIGNMENT >( buffer, chunkUnpackedAlignedSize, "CodecChunkBegin" );
	u8* chunk = chunkBegin;

	streamReader->Read( ToX64( chunkUnpackedAlignedSize ), chunkBegin );
#endif

#if CODEC_FRAME_COMPRESS != CODEC_FRAME_COMPRESS_TYPE_NONE && CODEC_FRAME_PACK_POSITIONS == 1
	if ( packedAlignedOffsetChunk > 0 )
	{
		V6_ASSERT( Codec_AlignToBufferSize( blockPosSize ) >= packedAlignedOffsetChunk );
		const u64 packedBlockPosAlignedSize = Codec_AlignToBufferSize( blockPosSize ) - packedAlignedOffsetChunk;

		BitStream_s bitStreamReader;
		BitStream_InitForRead( &bitStreamReader, (u64*)(chunk + packedAlignedOffsetChunk), (u32)(packedBlockPosAlignedSize * 8) );

		u32* blockPos = stack->newArray< u32 >( frameHeader.desc.blockCount, "CodecBlockPos" );
		Block_UnpackPositions( &bitStreamReader, blockPos, frameHeader.desc.blockCount );

        Codec_LogCompressedSize(chunk + packedAlignedOffsetChunk, packedBlockPosAlignedSize, "encodedBlockPos", allocator, stack);

		memcpy( chunk, blockPos, blockPosSize );
	}
#endif

	data->blockPos = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockPosSize );

	data->blockCellPresences0 = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellPresence0Size );

	data->blockCellPresences1 = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellPresence1Size );

	data->blockCellEndColors = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellEndColorSize );

	data->blockCellColorIndices0 = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellColorIndex0Size );

	data->blockCellColorIndices1 = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellColorIndex1Size );

	data->blockCellColorIndices2 = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellColorIndex2Size );

	data->blockCellColorIndices3 = (u32*)chunk;
	chunk += Codec_AlignToBufferSize( blockDataCellColorIndex3Size );
	
	data->blockRanges = (CodecBlockRange_s*)chunk;
	chunk += Codec_AlignToBufferSize( blockRangeSize );

    Codec_LogCellPresence(data->blockCellPresences0, data->blockCellPresences1, frameHeader.desc.blockCount);

    Codec_LogCompressedSize(data->blockPos, blockPosSize, "blockPos", allocator, stack);
    Codec_LogCompressedSize(data->blockCellPresences0, blockDataCellPresence0Size, "blockCellPresences0", allocator, stack);
    Codec_LogCompressedSize(data->blockCellPresences1, blockDataCellPresence1Size, "blockCellPresences1", allocator, stack);
    Codec_LogCompressedSize(data->blockCellEndColors, blockDataCellEndColorSize, "blockCellEndColors", allocator, stack);
    Codec_LogCompressedSize(data->blockCellColorIndices0, blockDataCellColorIndex0Size, "blockCellColorIndices0", allocator, stack);
    Codec_LogCompressedSize(data->blockCellColorIndices1, blockDataCellColorIndex1Size, "blockCellColorIndices1", allocator, stack);
    Codec_LogCompressedSize(data->blockCellColorIndices2, blockDataCellColorIndex2Size, "blockCellColorIndices2", allocator, stack);
    Codec_LogCompressedSize(data->blockCellColorIndices3, blockDataCellColorIndex3Size, "blockCellColorIndices3", allocator, stack);

	V6_ASSERT( ToU64( streamReader->GetPos() ) - beginPos == frameHeader.size );
	V6_ASSERT( chunk - chunkBegin == chunkUnpackedAlignedSize );

	return (u32)chunkUnpackedAlignedSize;
}

u32	Codec_ReadFrame( void** buffer, CStreamReaderWithBuffering* streamReader, CodecFrameDesc_s* desc, CodecFrameData_s* data, u32 frameRank, IAllocator* allocator, IStack* stack )
{
	V6_ASSERT( streamReader->GetBufferSize() == CODEC_CLUSTER_SIZE );
	return Codec_ReadFrame( buffer, (IStreamReader*)streamReader, desc, data, frameRank, allocator, stack );
}

bool Codec_WriteFrame( CStreamWriterWithBuffering* streamWriter, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, IStack* stack )
{
	V6_ASSERT( streamWriter->GetBufferSize() == CODEC_CLUSTER_SIZE );

	const u64 beginPos = ToU64( streamWriter->GetPos() );

	ScopedStack scopedStack( stack );

	if ( desc->flags & CODEC_FRAME_FLAG_MOTION )
	{
		V6_ASSERT( data == nullptr );
		V6_ASSERT( desc->blockCount == 0 );
		V6_ASSERT( desc->blockRangeCount == 0 );

		CodecFrameHeader_s frameHeader = {};
		{
			memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
			frameHeader.version = CODEC_FRAME_VERSION;
			frameHeader.size = (u32)sizeof( frameHeader );
			memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );
		}

		streamWriter->Write( &frameHeader, ToX64( sizeof( frameHeader ) ) );

		V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == frameHeader.size );

		return true;
	}
	
	V6_ASSERT( data != nullptr );

	const u64 blockPosSize = desc->blockCount * 4;
	const u64 blockDataCellPresence0Size = desc->blockCount * 4;
	const u64 blockDataCellPresence1Size = desc->blockCount * 4;
	const u64 blockDataCellEndColorSize = desc->blockCount * 4;
	const u64 blockDataCellColorIndex0Size = desc->blockCount * 4;
	const u64 blockDataCellColorIndex1Size = desc->blockCount * 4;
	const u64 blockDataCellColorIndex2Size = desc->blockCount * 4;
	const u64 blockDataCellColorIndex3Size = desc->blockCount * 4;
	V6_ASSERT( desc->blockRangeCount <= CODEC_RANGE_MAX_COUNT_PER_FRAME );
	const u64 blockRangeSize = desc->blockRangeCount * sizeof( CodecBlockRange_s );

	const u64 chunkUnpackedAlignedSize = 
		Codec_AlignToBufferSize( blockPosSize ) + 
		Codec_AlignToBufferSize( blockDataCellPresence0Size ) + 
		Codec_AlignToBufferSize( blockDataCellPresence1Size ) + 
		Codec_AlignToBufferSize( blockDataCellEndColorSize ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex0Size ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex1Size ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex2Size ) + 
		Codec_AlignToBufferSize( blockDataCellColorIndex3Size ) + 
		Codec_AlignToBufferSize( blockRangeSize );
	CBufferWriter chunkWriter( stack->alloc( chunkUnpackedAlignedSize, "CodecChunkWriter" ), ToX64( chunkUnpackedAlignedSize ) );

#if CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_NONE
	const u64 chunkUncompressedAlignedSize = chunkUnpackedAlignedSize;
	const u64 chunkCompressedSize = chunkUnpackedAlignedSize;
	u8* chunkCompressed = (u8*)chunkWriter.GetBuffer();

	chunkWriter.WriteAligned( data->blockPos, ToX64( blockPosSize ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellPresences0, ToX64( blockDataCellPresence0Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellPresences1, ToX64( blockDataCellPresence1Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellEndColors, ToX64( blockDataCellEndColorSize ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices0, ToX64( blockDataCellColorIndex0Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices1, ToX64( blockDataCellColorIndex1Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices2, ToX64( blockDataCellColorIndex2Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices3, ToX64( blockDataCellColorIndex3Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockRanges, ToX64( blockRangeSize ), CODEC_BUFFER_ALIGNMENT );
#else

#if CODEC_FRAME_PACK_POSITIONS == 1
	const u64 packedBlockPosMaxSize = Block_ComputeBufferMaxSizeForPackingPositions( desc->blockCount );
	u64* packedBlockPos = stack->newArray< u64 >( packedBlockPosMaxSize / 8, "CodecPackedBlockPos" );
	BitStream_s bitStreamWriter;
	BitStream_InitForWrite( &bitStreamWriter, packedBlockPos, (u32)(packedBlockPosMaxSize * 8) );
	Block_PackPositions( &bitStreamWriter, data->blockPos, desc->blockCount );
	const u32 packedBlockPosSize = BitStream_GetSize( &bitStreamWriter );
	if ( packedBlockPosSize < blockPosSize )
	{
		chunkWriter.WriteAligned( packedBlockPos, ToX64( packedBlockPosSize ), CODEC_BUFFER_ALIGNMENT );
	}
	else
#endif
	{
		chunkWriter.WriteAligned( data->blockPos, ToX64( blockPosSize ), CODEC_BUFFER_ALIGNMENT );
	}

	chunkWriter.WriteAligned( data->blockCellPresences0, ToX64( blockDataCellPresence0Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellPresences1, ToX64( blockDataCellPresence1Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellEndColors, ToX64( blockDataCellEndColorSize ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices0, ToX64( blockDataCellColorIndex0Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices1, ToX64( blockDataCellColorIndex1Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices2, ToX64( blockDataCellColorIndex2Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockCellColorIndices3, ToX64( blockDataCellColorIndex3Size ), CODEC_BUFFER_ALIGNMENT );
	chunkWriter.WriteAligned( data->blockRanges, ToX64( blockRangeSize ), CODEC_BUFFER_ALIGNMENT );

	const u64 chunkUncompressedAlignedSize = ToU64( chunkWriter.GetPos() );

#if CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_LZ4
	const u64 chunkCompressedMaxSize = LZ4_compressBound( chunkUncompressedAlignedSize );
	u8* chunkCompressed = (u8*)stack->alloc( chunkCompressedMaxSize );
	const u64 chunkCompressedSize = LZ4_compress_HC( (char*)chunkWriter.GetBuffer(), (char*)chunkCompressed, (int)chunkUncompressedAlignedSize, (int)chunkCompressedMaxSize, CODEC_LZ4_COMPRESSION_LEVEL );
#elif CODEC_FRAME_COMPRESS == CODEC_FRAME_COMPRESS_TYPE_ZSTD
	const u64 chunkCompressedMaxSize = ZSTD_compressBound( chunkUncompressedAlignedSize );
	u8* chunkCompressed = (u8*)stack->alloc( chunkCompressedMaxSize, "CodecChunkCompressed" );
	const u64 chunkCompressedSize = ZSTD_compress( chunkCompressed, chunkCompressedMaxSize, chunkWriter.GetBuffer(), chunkUncompressedAlignedSize, CODEC_ZSTD_COMPRESSION_LEVEL );
#endif

	if ( chunkCompressedSize == 0 )
	{
		V6_ERROR( "Compression failed.\n" );
		return false;
	}
	V6_MSG( "Compression: %d KB / %d KB (%5.2f%%)\n", DivKB( chunkCompressedSize ), DivKB( chunkUnpackedAlignedSize ), chunkCompressedSize * 100.0f / chunkUnpackedAlignedSize );
#endif
	
	const u64 frameBufferSize = sizeof( CodecFrameHeader_s ) + chunkCompressedSize;

	CodecFrameHeader_s frameHeader = {};
	{
		memcpy( frameHeader.magic, CODEC_FRAME_MAGIC, 4 );
		frameHeader.version = CODEC_FRAME_VERSION;
		frameHeader.size = (u32)frameBufferSize;
		frameHeader.uncompressedDataAlignedSize = (u32)chunkUncompressedAlignedSize;
		memcpy( &frameHeader.desc, desc, sizeof( frameHeader.desc ) );
	}

	streamWriter->Write( &frameHeader, ToX64( sizeof( frameHeader ) ) );
	streamWriter->Write( chunkCompressed, ToX64( chunkCompressedSize ) );

	V6_ASSERT( ToU64( streamWriter->GetPos() ) - beginPos == frameHeader.size );

	return true;
}

u32 Codec_GetSupportedFrameRates( const u32** frameRates )
{
	static u32 s_supportedFrameRates[] = { 1, 30, 45, 60, 90, 120 };
	
	*frameRates = s_supportedFrameRates;
	return V6_ARRAY_COUNT( s_supportedFrameRates );
}

u32 Codec_GetDefaultFrameRate()
{
	const u32 frameRate = 90;

	V6_ASSERT( Codec_IsFrameRateSupported( frameRate ) );

	return frameRate;
}

bool Codec_IsFrameRateSupported( u32 frameRate )
{
	const u32* supportedFrameRates;
	const u32 frameRateCount = Codec_GetSupportedFrameRates( &supportedFrameRates );
	
	for ( u32 frameRateID = 0; frameRateID < frameRateCount; ++frameRateID )
	{
		if ( supportedFrameRates[frameRateID] == frameRate )
			return true;
	}
	
	return false;
}

#if CODEC_DEBUG_SIMULATE_STREAM_ERROR == 1
bool Codec_Error()
{
#if 1
	static u32 s_state = RandInt();
	s_state = RandXORShift( s_state );
	if ( (s_state & 0x7FFF) == 17 )
	{
		V6_MSG( "Codec Random Error %u\n", s_state );
		return true;
	}
#else
	static const u32 s_errorRank = 18;
	static u32 s_rank = 0;
	if ( Atomic_Inc( &s_rank ) == s_errorRank )
	{
		V6_MSG( "Codec Rank Error %d\n", s_errorRank );
		return true;
	}
#endif

	return false;
}
#endif

END_V6_NAMESPACE
