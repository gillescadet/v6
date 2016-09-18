/*V6*/

#include <v6/core/common.h>

#include <v6/codec/compression.h>
#include <v6/codec/encoder.h>
#include <v6/codec/codec.h>
#include <v6/core/bit.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/memory.h>
#include <v6/core/plot.h>
#include <v6/core/process.h>
#include <v6/core/stream.h>
#include <v6/core/string.h>
#include <v6/core/thread.h>
#include <v6/core/time.h>
#include <v6/core/vec3i.h>

#if V6_UE4_PLUGIN == 0

#include <lz4/lib/lz4.h>
#include <lz4/lib/lz4hc.h>

#define ENCODER_DUMP_BLOCKS				0
#define ENCODER_SKIP_WRITING			0
#define ENCODER_DUMP_RANGES				0

#define ENCODER_INVALID_ID				0xFFFFFFFF
#define ENCODER_EMPTY_RANGE				0xFFFFFFFF
#define ENCODER_EMPTY_CELL				0xFFFFFFFF

#define ENCODER_THREAD_COUNT			4
#define ENCODER_BLOCK_PER_DATA_CHUNK	MulKB( 1 )
#define ENCODER_LZ4_COMPRESSION_LEVEL	4

BEGIN_V6_NAMESPACE

#define INTERLEAVE_S( S, SHIFT, OFFSET )	(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )			INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )			INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )			INTERLEAVE_S( Z, SHIFT, 2 )

struct Context_s;

struct BlockDataChunk_s
{
	u64			filePos;
	u32*		offsets;
	union
	{
		u32*		decompressedRGBA;
		void*		compressedRGBA;
	};
	u32			blockCount;
	u32			compressedSize;
	u32			decompressedSize;
};

struct BlockDataChunkJob_s
{
	IAllocator*			heap;
	Stack				stack;
	BlockDataChunk_s*	firstChunk;
	u8*					compressedBuffer;
	u32*				compressedBufferSize;
	u32					compressedBufferMaxSize;
	bool				success;
};

enum BlockClusterLinkResult_e
{
	BLOCK_CLUSTER_LINK_RESULT_NOT_EQUAL,
	BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX,
	BLOCK_CLUSTER_LINK_RESULT_EQUAL
};

struct BlockClusterBase_s
{
	u8					isEncoded;
	u8					sharedFrameCount;
	u8					cellDiffCount;
	u8					pad2;
	u64					cellPresence;
};

struct BlockClusterEncoded_s
{
	u8					isEncoded;
	u8					sharedFrameCount;
	u8					pad1;
	u8					pad2;
	EncodedBlockEx_s	encodedData;
};

struct BlockClusterCell_s
{
	u16		sumR;
	u8		minR;
	u8		maxR;

	u16		sumG;
	u8		minG;
	u8		maxG;

	u16		sumB;
	u8		minB;
	u8		maxB;

	u32		count;
};

V6_STATIC_ASSERT( sizeof( BlockClusterBase_s ) + CODEC_COLOR_COUNT_TOLERANCE * sizeof( BlockClusterCell_s ) >= sizeof( BlockClusterEncoded_s ) );

struct BlockCluster_s : BlockClusterBase_s
{
	BlockClusterCell_s	cells[CODEC_CELL_MAX_COUNT];
};

struct Block_s
{
	void					SetCluster( BlockCluster_s* cluster )
	{
		opaquePointerToClusterOrEncodedBlock = cluster;
		cluster->isEncoded = false;
	}

	BlockCluster_s*			GetCluster()
	{
		BlockCluster_s* cluster = (BlockCluster_s*)opaquePointerToClusterOrEncodedBlock;
		if ( cluster && !cluster->isEncoded )
			return cluster;
		return nullptr;
	}
	
	const EncodedBlockEx_s*		GetEncodedBlock() const
	{
		BlockClusterEncoded_s* clusterEncoded = (BlockClusterEncoded_s*)opaquePointerToClusterOrEncodedBlock;
		if ( clusterEncoded && clusterEncoded->isEncoded )
			return &clusterEncoded->encodedData;
		return nullptr;
	}

	const BlockClusterEncoded_s*	GetEncodedCluster() const
	{
		BlockClusterEncoded_s* clusterEncoded = (BlockClusterEncoded_s*)opaquePointerToClusterOrEncodedBlock;
		if ( clusterEncoded && clusterEncoded->isEncoded )
			return clusterEncoded;
		return nullptr;
	}

	void*					opaquePointerToClusterOrEncodedBlock;
	u32						thisBlockOrder;
	u32						nextBlockOrder;
	u64						key;
	union
	{
		u32					mip4_none1_pos27;
		u32					sign1_axis2_z11_y9_x9;
		u32					packedBlockPos;
	};
	u8						linked;
	u8						frameRank;
	u8						sharedFrameCount;
	u8						bucket;
};

struct RawFrameBlockCache_s
{
	u8*					chunkBuffer;
	u32					lastBlockOrder;
	u32					lastBlockDataChunkID;
	u32					lastBlockCellRGBA[64];
	u64					lastBlockCellPresence;
};

struct RawFrame_s
{
	Block_s*				blocks;
	u32*					blockIDs;
	BlockDataChunk_s*		blockDataChunks;
	u32						blockCount;
	u32						refFrameRank;
	Vec3					gridOrigin;
	float					gridYaw;
	Vec3i					gridMin[CODEC_GRID_MAX_COUNT];
	Vec3i					gridMax[CODEC_GRID_MAX_COUNT];
	u32						blockCountPerGrid[CODEC_GRID_MAX_COUNT];
	u32						blockOffsetPerGrid[CODEC_GRID_MAX_COUNT];

	u32						sharedBlockCount;
	struct Shared_s
	{
		u32					blockCount;
		u32					rangeIDs[CODEC_GRID_MAX_COUNT];
		u32					blockCountPerGrid[CODEC_GRID_MAX_COUNT];
		u32					blockOffsetPerGrid[CODEC_GRID_MAX_COUNT];
	}						shareds[CODEC_FRAME_MAX_COUNT];

	CFileReader				cacheFileReader;
	RawFrameBlockCache_s	threadCaches[ENCODER_THREAD_COUNT];
};

struct RawFrameMergeJob_s
{
	Context_s*			context;
	u32					frameRank;
	const u32*			blockIDs;
};

struct ContextStream_s
{
	IAllocator*			heap;
	Stack*				stack;
	CodecStreamDesc_s	desc;
	u32					gridMacroWidth;
	u32					gridMacroHalfWidth;
	u32					gridCount;
};

struct Context_s
{
	Mutex_s				mainLock;
	ContextStream_s*	stream;
	IAllocator*			heap;
	IStack*				stack;
	RawFrame_s*			frames;
	WorkerThread_s		threads[ENCODER_THREAD_COUNT];
	BlockAllocator_s	threadBlockAllocators[ENCODER_THREAD_COUNT];
	Vec3i				gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i				gridMax[CODEC_MIP_MAX_COUNT];
	CodecRange_s		rangeDefs[CODEC_RANGE_MAX_COUNT];
	u32					rangeDefCount;
	u32					frameCount;
	u32					compressionQuality;
	u32					blockCountPerSequence;
	u32					resolvedBlockPerSequence;
	u32					unresolvedBlockPerSequence;
};

static void ShowProgress()
{
	static u32 s_step = 0;
	const char cars[] = { '/', '|', '\\', '-' }; 
	printf( "\r%c", cars[s_step % 4] );
	++s_step;
}

static void HideProgress()
{
	printf( "\r \r" );
}

static void ComputeCellCoords_Mip( Vec3* pMin, Vec3* pMax, u32 blockPos, u32 gridMacroShift, u32 cellPos, const Vec3* gridCenter, float cellSize, float gridScale )
{
	const u32 gridMacroMask = (1 << gridMacroShift) - 1;
	const u32 blockX = (u32)((blockPos >> (gridMacroShift * 0)) & gridMacroMask);
	const u32 blockY = (u32)((blockPos >> (gridMacroShift * 1)) & gridMacroMask);
	const u32 blockZ = (u32)((blockPos >> (gridMacroShift * 2)) & gridMacroMask);
	const u32 cellX = (u32)((cellPos >> 0) & 3);
	const u32 cellY = (u32)((cellPos >> 2) & 3);
	const u32 cellZ = (u32)((cellPos >> 4) & 3);

	const Vec3u cellCoords = Vec3u_Make( (blockX << 2) | cellX, (blockY << 2) | cellY, (blockZ << 2) | cellZ );

	pMin->x = gridCenter->x + (cellCoords.x * cellSize ) - gridScale;
	pMin->y = gridCenter->y + (cellCoords.y * cellSize ) - gridScale;
	pMin->z = gridCenter->z + (cellCoords.z * cellSize ) - gridScale;
	
	*pMax = *pMin + cellSize;
}

static void ComputeCellCoords_Onion( Vec3* pMin, Vec3* pMax, u32 sign1_axis2_z11_y9_x9, u32 gridMacroShift, u32 cellPos, const Vec3* gridCenter, float gridMinScale, float invMacroPeriodWidth, float invMacroGridWidth )
{
	const u32 sign = (sign1_axis2_z11_y9_x9 >> 31) & 1;
	const u32 axis = (sign1_axis2_z11_y9_x9 >> 29) & 3;

	Vec3u blockCoords;
	blockCoords.x = (sign1_axis2_z11_y9_x9 >>  0) & 0x1FF;
	blockCoords.y = (sign1_axis2_z11_y9_x9 >>  9) & 0x1FF;
	blockCoords.z = (sign1_axis2_z11_y9_x9 >> 18) & 0x7FF;

	Vec3 blockPosMinGS, blockPosMaxGS;

	blockPosMinGS.z = gridMinScale * exp2f( (blockCoords.z + 0.0f) * invMacroPeriodWidth );
	blockPosMaxGS.z = gridMinScale * exp2f( (blockCoords.z + 1.0f) * invMacroPeriodWidth );
	
	blockPosMinGS.x = ((blockCoords.x + 0.0f) * invMacroGridWidth * 2.0f - 1.0f) * blockPosMaxGS.z;
	blockPosMaxGS.x = ((blockCoords.x + 1.0f) * invMacroGridWidth * 2.0f - 1.0f) * blockPosMaxGS.z;

	blockPosMinGS.y = ((blockCoords.y + 0.0f) * invMacroGridWidth * 2.0f - 1.0f) * blockPosMaxGS.z;
	blockPosMaxGS.y = ((blockCoords.y + 1.0f) * invMacroGridWidth * 2.0f - 1.0f) * blockPosMaxGS.z;

	const float signedMinZ = sign ? -blockPosMaxGS.z : blockPosMinGS.z;
	const float signedMaxZ = sign ? -blockPosMinGS.z : blockPosMaxGS.z;

	Vec3 blockPosMinRS, blockPosMaxRS;

	if ( axis == 0 )
	{
		blockPosMinRS.x = signedMinZ;
		blockPosMinRS.y = blockPosMinGS.x;
		blockPosMinRS.z = blockPosMinGS.y;

		blockPosMaxRS.x = signedMaxZ;
		blockPosMaxRS.y = blockPosMaxGS.x;
		blockPosMaxRS.z = blockPosMaxGS.y;
	}
	else if ( axis == 1 )
	{
		blockPosMinRS.y = signedMinZ;
		blockPosMinRS.z = blockPosMinGS.x;
		blockPosMinRS.x = blockPosMinGS.y;

		blockPosMaxRS.y = signedMaxZ;
		blockPosMaxRS.z = blockPosMaxGS.x;
		blockPosMaxRS.x = blockPosMaxGS.y;
	}
	else
	{
		blockPosMinRS.z = signedMinZ;
		blockPosMinRS.x = blockPosMinGS.x;
		blockPosMinRS.y = blockPosMinGS.y;

		blockPosMaxRS.z = signedMaxZ;
		blockPosMaxRS.x = blockPosMaxGS.x;
		blockPosMaxRS.y = blockPosMaxGS.y;
	}

	const Vec3 cellSize = (blockPosMaxRS - blockPosMinRS) * 0.25f;
	const Vec3 invCellSize = cellSize.Rcp();

	const u32 cellX = (u32)((cellPos >> 0) & 3);
	const u32 cellY = (u32)((cellPos >> 2) & 3);
	const u32 cellZ = (u32)((cellPos >> 4) & 3);

	pMin->x = gridCenter->x + blockPosMinRS.x + (cellX * cellSize.x);
	pMin->y = gridCenter->y + blockPosMinRS.y + (cellY * cellSize.y);
	pMin->z = gridCenter->z + blockPosMinRS.z + (cellZ * cellSize.z);

	*pMax = *pMin + cellSize;
}

static u64 ComputeKeyFromMipAndBlockPos( u32 mip, u32 blockPos, Vec3i blockPosTranslation )
{
	V6_ASSERT( mip < CODEC_MIP_MAX_COUNT );

	const u64 x = (u64)((blockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 0)) & CODEC_MIP_MACRO_XYZ_BIT_MASK) + blockPosTranslation.x;
	const u64 y = (u64)((blockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 1)) & CODEC_MIP_MACRO_XYZ_BIT_MASK) + blockPosTranslation.y;
	const u64 z = (u64)((blockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 2)) & CODEC_MIP_MACRO_XYZ_BIT_MASK) + blockPosTranslation.z;

	const u32 maxValue = 0xFFFFF;
	V6_ASSERT( x <= maxValue );
	V6_ASSERT( y <= maxValue );
	V6_ASSERT( z <= maxValue );

	u64 key = (u64)mip << 60;
	for ( u32 shift = 0; shift < 20; ++shift )
	{
		key |= INTERLEAVE_X( x, shift );
		key |= INTERLEAVE_Y( y, shift );
		key |= INTERLEAVE_Z( z, shift );
	}

	return key;
}

static u64 ComputeKeyFromFaceAndBlockPos( u32 face, u32 blockPos )
{
	V6_ASSERT( face < CODEC_FACE_MAX_COUNT );

	const u64 x = (blockPos >>  0) & 0x1FF;
	const u64 y = (blockPos >>  9) & 0x1FF;
	const u64 z = (blockPos >> 18) & 0x7FF;

	const u32 maxValue = 0xFFFFF;
	V6_ASSERT( x <= maxValue );
	V6_ASSERT( y <= maxValue );
	V6_ASSERT( z <= maxValue );

	u64 key = (u64)face << 61;
	for ( u32 shift = 0; shift < 20; ++shift )
	{
		key |= INTERLEAVE_X( x, shift );
		key |= INTERLEAVE_Y( y, shift );
		key |= INTERLEAVE_Z( z, shift );
	}

	return key;
}


static void BlockDataChunk_Write( IStreamWriter* streamWriter, BlockDataChunk_s* chunk, Context_s* context )
{
	chunk->filePos = ToU64( streamWriter->GetPos() );

	ScopedStack scopedStack( context->stack );

	const u64 offsetSize = ENCODER_BLOCK_PER_DATA_CHUNK * sizeof( u32 );
	const u64 chunkSize = offsetSize + chunk->compressedSize;
	const u64 chunkAlignedSize = Codec_AlignToClusterSize( offsetSize + chunk->compressedSize );
	void* chunkData = Codec_AllocToClusterSizeAndFillPaddingWithZero( nullptr, chunkAlignedSize, context->stack );
	
	CBufferWriter bufferWriter( chunkData, ToX64( chunkAlignedSize ) );
	bufferWriter.Write( chunk->offsets, ToX64( offsetSize ) );
	bufferWriter.Write( chunk->compressedRGBA, ToX64( chunk->compressedSize ) );
	
	chunk->offsets = nullptr;
	chunk->compressedRGBA = nullptr;

	streamWriter->Write( chunkData, ToX64( chunkAlignedSize ) );
}

static bool BlockDataChunk_Read( IStreamReader* streamReader, BlockDataChunk_s* chunk, u8* chunkBuffer, Context_s* context )
{
	ScopedStack scopedStack( context->stack );

	const u64 offsetSize = ENCODER_BLOCK_PER_DATA_CHUNK * sizeof( u32 );
	const u64 chunkSize = offsetSize + chunk->compressedSize;
	const u64 chunkAlignedSize = Codec_AlignToClusterSize( offsetSize + chunk->compressedSize );
	u8* chunkData = (u8*)Codec_AllocToClusterSizeAndFillPaddingWithZero( nullptr, chunkAlignedSize, context->stack );

	streamReader->SetPos( ToX64( chunk->filePos ) );
	streamReader->Read( ToX64( chunkAlignedSize ), chunkData );
	
	chunk->offsets = (u32*)chunkBuffer;
	memcpy( chunk->offsets, chunkData, offsetSize );
	
	chunk->decompressedRGBA = (u32*)(chunkBuffer + offsetSize);
	if ( LZ4_decompress_fast( (char*)(chunkData + offsetSize), (char*)chunk->decompressedRGBA, chunk->decompressedSize ) != chunk->compressedSize )
	{
		V6_ERROR( "LZ4 decompression failed.\n" );
		return false;
	}

	return true;
}

static int Block_CompareKey( void* framePointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	RawFrame_s* frame = (RawFrame_s*)framePointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	return frame->blocks[blockID0].key < frame->blocks[blockID1].key ? -1 : 1;
}

static int Block_CompareBySharedFrameCountThenByKey( void* framePointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	RawFrame_s* frame = (RawFrame_s*)framePointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	if ( frame->blocks[blockID0].sharedFrameCount < frame->blocks[blockID1].sharedFrameCount )
		return -1;

	if ( frame->blocks[blockID0].sharedFrameCount > frame->blocks[blockID1].sharedFrameCount )
		return 1;

	return frame->blocks[blockID0].key < frame->blocks[blockID1].key ? -1 : 1;
}

static void Block_GetColors( const u32** cellRGBA, u64* cellPresence, const Block_s* block, Context_s* context, u32 threadID )
{
	RawFrame_s* rawFrame = &context->frames[block->frameRank];
	RawFrameBlockCache_s* cache = &rawFrame->threadCaches[threadID];
	
	if ( block->thisBlockOrder != cache->lastBlockOrder )
	{
		const u32 chunkID = block->thisBlockOrder / ENCODER_BLOCK_PER_DATA_CHUNK;
		const u32 chunkCount = (rawFrame->blockCount + ENCODER_BLOCK_PER_DATA_CHUNK - 1) / ENCODER_BLOCK_PER_DATA_CHUNK;
		V6_ASSERT( chunkID < chunkCount );
		BlockDataChunk_s* chunk = &rawFrame->blockDataChunks[chunkID];

		if ( chunkID != cache->lastBlockDataChunkID )
		{
			Mutex_Lock( &context->mainLock );

			if ( !BlockDataChunk_Read( &rawFrame->cacheFileReader, chunk, cache->chunkBuffer, context ) )
			{
				Mutex_Unlock( &context->mainLock );
				exit( 1 );
			}
			Mutex_Unlock( &context->mainLock );
			// V6_MSG( "Decompress chunk F%02d_%04d\n", block->frameRank, chunkID );
			cache->lastBlockDataChunkID = chunkID;
		}

		const u32 blockOrderInChunk = block->thisBlockOrder - chunkID * ENCODER_BLOCK_PER_DATA_CHUNK;
		const u32* blockCellRGBA = chunk->decompressedRGBA + chunk->offsets[blockOrderInChunk];
		const u32 cellPerBucketCount = 1 << (block->bucket + 2);
			
		cache->lastBlockCellPresence = 0;

		for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
		{
			const u32 rgba = blockCellRGBA[cellID];
			if ( rgba == ENCODER_EMPTY_CELL )
				break;
				
			const u32 cellPos = rgba & 0xFF;
			V6_ASSERT( cellPos < 64 );

			cache->lastBlockCellRGBA[cellPos] = rgba;
			cache->lastBlockCellPresence |= 1ll << cellPos;
		}

		cache->lastBlockOrder = block->thisBlockOrder;
	}

	*cellRGBA = cache->lastBlockCellRGBA;
	*cellPresence = cache->lastBlockCellPresence;
}

static u32 Block_GetGrid( const Block_s* block, Context_s* context )
{
	return (context->stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) != 0 ? (block->mip4_none1_pos27 >> 28) : (block->sign1_axis2_z11_y9_x9 >> 29);
}

static u32 Block_GetPos( const Block_s* block, Context_s* context )
{
	return (context->stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) != 0 ? (block->mip4_none1_pos27 & 0x07FFFFFF) : (block->sign1_axis2_z11_y9_x9 & 0x1FFFFFFF);
}

static Block_s* Block_GetLinkedBlock( const Block_s* block, Context_s* context )
{
	if ( block->nextBlockOrder == (u32)-1 )
		return nullptr;

	const u32 nextFrameRank = block->frameRank + 1;
	V6_ASSERT( nextFrameRank < context->frameCount );
	const RawFrame_s* nextFrame = &context->frames[nextFrameRank];
	V6_ASSERT( block->nextBlockOrder < nextFrame->blockCount );
	const u32 nextFrameBlockID = nextFrame->blockIDs[block->nextBlockOrder];
	V6_ASSERT( nextFrameBlockID < nextFrame->blockCount );
	return &nextFrame->blocks[nextFrameBlockID];
}

static void Block_DetachLinkedBlock( Block_s* block, Context_s* context )
{
	V6_ASSERT( block->nextBlockOrder != (u32)-1 );
	Block_GetLinkedBlock( block, context )->linked = false;
	block->nextBlockOrder = (u32)-1;
}

static BlockCluster_s* BlockCluster_Create( BlockCluster_s* cluster, const Block_s* block, Context_s* context, u32 threadID )
{
	const u32* blockCellRGBA;
	u64 blockCellPresence;
	Block_GetColors( &blockCellRGBA, &blockCellPresence, block, context, threadID );

	const u32 blockCellCount = Bit_GetBitHighCount( blockCellPresence );
	const u32 allocCount = Min( Bit_GetBitHighCount( blockCellPresence ) + CODEC_COLOR_COUNT_TOLERANCE, CODEC_CELL_MAX_COUNT );
	const u32 allocSize = sizeof( BlockClusterBase_s ) + allocCount * sizeof( BlockClusterCell_s );

	if ( cluster == nullptr )
		cluster = (BlockCluster_s*)BlockAllocator_Alloc( &context->threadBlockAllocators[threadID], allocSize );
	memset( cluster, 0, allocSize );

	cluster->cellPresence = blockCellPresence;

	u32 cellID = 0;
	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( blockCellPresence );
		blockCellPresence -= 1ll << cellPos;

		const u32 refR = (blockCellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (blockCellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (blockCellRGBA[cellPos] >>  8) & 0xFF;

		cluster->cells[cellID].sumR  = refR;
		cluster->cells[cellID].sumG  = refG;
		cluster->cells[cellID].sumB  = refB;

		cluster->cells[cellID].minR  = refR;
		cluster->cells[cellID].minG  = refG;
		cluster->cells[cellID].minB  = refB;

		cluster->cells[cellID].maxR  = refR;
		cluster->cells[cellID].maxG  = refG;
		cluster->cells[cellID].maxB  = refB;

		cluster->cells[cellID].count = 1;

		++cellID;
	}
	while ( blockCellPresence != 0 );
	
	return cluster;
}

static void BlockCluster_Copy( BlockCluster_s* dst, const BlockCluster_s* src )
{
	const u32 blockCellCount = Bit_GetBitHighCount( src->cellPresence );
	const u32 clusterSize = sizeof( BlockClusterBase_s ) + blockCellCount * sizeof( BlockClusterCell_s );
	memcpy( dst, src, clusterSize );
}

static void BlockCluster_ResolveColors( const BlockCluster_s* cluster, Block_s* block, Context_s* context, u32 threadID )
{
	u64 cellPresence = cluster->cellPresence;
	V6_ASSERT( cellPresence != 0 );
	
	u32 blockCellRGBA[64];
	u32 blockCellCount = 0;
	
	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( cellPresence );
		cellPresence -= 1ll << cellPos;

		const u32 avgR = cluster->cells[blockCellCount].sumR / cluster->cells[blockCellCount].count;
		const u32 avgG = cluster->cells[blockCellCount].sumG / cluster->cells[blockCellCount].count;
		const u32 avgB = cluster->cells[blockCellCount].sumB / cluster->cells[blockCellCount].count;
		
		blockCellRGBA[blockCellCount] = (avgR << 24) | (avgG << 16) | (avgB << 8) | cellPos;

		++blockCellCount;
	}
	while ( cellPresence != 0 );

	BlockClusterEncoded_s* clusterEncoded = (BlockClusterEncoded_s*)cluster; // reuse cluster memory
	memset( &clusterEncoded->encodedData, 0, sizeof( EncodedBlockEx_s ) );
	Block_Encode_Optimize( &clusterEncoded->encodedData, blockCellRGBA, blockCellCount, context->compressionQuality );
	clusterEncoded->isEncoded = true;

	Atomic_Inc( &context->resolvedBlockPerSequence );
}

static BlockClusterLinkResult_e BlockCluster_LinkColors( BlockCluster_s* cluster, Block_s* linkedBlock, Context_s* context, u32 threadID )
{
	const u32* linkedBlockCellRGBA;
	u64 linkedBlockCellPresence;
	Block_GetColors( &linkedBlockCellRGBA, &linkedBlockCellPresence, linkedBlock, context, threadID );

	cluster->cellDiffCount += Bit_GetBitHighCount( cluster->cellPresence ^ linkedBlockCellPresence );
	if ( cluster->cellDiffCount > CODEC_COLOR_COUNT_TOLERANCE )
		return BLOCK_CLUSTER_LINK_RESULT_NOT_EQUAL;

	u32 clusterBlockCellR[64];
	u32 clusterBlockCellG[64];
	u32 clusterBlockCellB[64];
	u32 clusterBlockCellMinR[64];
	u32 clusterBlockCellMinG[64];
	u32 clusterBlockCellMinB[64];
	u32 clusterBlockCellMaxR[64];
	u32 clusterBlockCellMaxG[64];
	u32 clusterBlockCellMaxB[64];
	u32 clusterBlockCellCount[64] = {};
	
	// unpack

	{
		u64 clusterBlockCellPresence = cluster->cellPresence;
		u32 cellID = 0;
		do
		{
			const u32 cellPos = Bit_GetFirstBitHigh( clusterBlockCellPresence );
			clusterBlockCellPresence -= 1ll << cellPos;

			clusterBlockCellR[cellPos] = cluster->cells[cellID].sumR;
			clusterBlockCellG[cellPos] = cluster->cells[cellID].sumG;
			clusterBlockCellB[cellPos] = cluster->cells[cellID].sumB;

			clusterBlockCellMinR[cellPos] = cluster->cells[cellID].minR;
			clusterBlockCellMinG[cellPos] = cluster->cells[cellID].minG;
			clusterBlockCellMinB[cellPos] = cluster->cells[cellID].minB;
			
			clusterBlockCellMaxR[cellPos] = cluster->cells[cellID].maxR;
			clusterBlockCellMaxG[cellPos] = cluster->cells[cellID].maxG;
			clusterBlockCellMaxB[cellPos] = cluster->cells[cellID].maxB;
			
			clusterBlockCellCount[cellPos] = cluster->cells[cellID].count;

			++cellID;
		}
		while ( clusterBlockCellPresence != 0 );
	}

	// compare linked block against average cluster

	{
		u64 blockCellPresence = linkedBlockCellPresence;
		do
		{
			const u32 cellPos = Bit_GetFirstBitHigh( blockCellPresence );
			blockCellPresence -= 1ll << cellPos;

			const u32 refR = (linkedBlockCellRGBA[cellPos] >> 24) & 0xFF;
			const u32 refG = (linkedBlockCellRGBA[cellPos] >> 16) & 0xFF;
			const u32 refB = (linkedBlockCellRGBA[cellPos] >>  8) & 0xFF;

			if ( clusterBlockCellCount[cellPos] == 0 )
			{
				clusterBlockCellR[cellPos] = refR;
				clusterBlockCellG[cellPos] = refG;
				clusterBlockCellB[cellPos] = refB;

				clusterBlockCellMinR[cellPos] = refR;
				clusterBlockCellMinG[cellPos] = refG;
				clusterBlockCellMinB[cellPos] = refB;
				
				clusterBlockCellMaxR[cellPos] = refR;
				clusterBlockCellMaxG[cellPos] = refG;
				clusterBlockCellMaxB[cellPos] = refB;
			}
			else
			{
				clusterBlockCellR[cellPos] += refR;
				clusterBlockCellG[cellPos] += refG;
				clusterBlockCellB[cellPos] += refB;

				clusterBlockCellMinR[cellPos] = Min( clusterBlockCellMinR[cellPos], refR );
				clusterBlockCellMinG[cellPos] = Min( clusterBlockCellMinG[cellPos], refG );
				clusterBlockCellMinB[cellPos] = Min( clusterBlockCellMinB[cellPos], refB );

				clusterBlockCellMaxR[cellPos] = Max( clusterBlockCellMaxR[cellPos], refR );
				clusterBlockCellMaxG[cellPos] = Max( clusterBlockCellMaxG[cellPos], refG );
				clusterBlockCellMaxB[cellPos] = Max( clusterBlockCellMaxB[cellPos], refB );
			}
			++clusterBlockCellCount[cellPos];

			const u32 avgR = clusterBlockCellR[cellPos] / clusterBlockCellCount[cellPos];
			const u32 avgG = clusterBlockCellG[cellPos] / clusterBlockCellCount[cellPos];
			const u32 avgB = clusterBlockCellB[cellPos] / clusterBlockCellCount[cellPos];

			if ( Abs( (int)(refR - avgR) ) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_EQUAL;
			if ( Abs( (int)(refG - avgG) ) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_EQUAL;
			if ( Abs( (int)(refB - avgB) ) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_EQUAL;

		} while ( blockCellPresence != 0 );
	}

	// compare linked block against min/max cluster

	{
		u64 blockCellPresence = linkedBlockCellPresence;
		do
		{
			const u32 cellPos = Bit_GetFirstBitHigh( blockCellPresence );
			blockCellPresence -= 1ll << cellPos;

			const u32 avgR = clusterBlockCellR[cellPos] / clusterBlockCellCount[cellPos];
			const u32 avgG = clusterBlockCellG[cellPos] / clusterBlockCellCount[cellPos];
			const u32 avgB = clusterBlockCellB[cellPos] / clusterBlockCellCount[cellPos];

			if ( (int)(avgR - clusterBlockCellMinR[cellPos]) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX;
			if ( (int)(avgG - clusterBlockCellMinG[cellPos]) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX;
			if ( (int)(avgB - clusterBlockCellMinB[cellPos]) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX;

			if ( (int)(clusterBlockCellMaxR[cellPos] - avgR) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX;
			if ( (int)(clusterBlockCellMaxG[cellPos] - avgG) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX;
			if ( (int)(clusterBlockCellMaxB[cellPos] - avgB) > CODEC_COLOR_ERROR_TOLERANCE )
				return BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX;

		} while ( blockCellPresence != 0 );
	}
	
	// validate

	cluster->cellPresence |= linkedBlockCellPresence;
	++cluster->sharedFrameCount;

	// repack

	{
		u64 clusterBlockCellPresence = cluster->cellPresence;
		u32 cellID = 0;
		do
		{
			const u32 cellPos = Bit_GetFirstBitHigh( clusterBlockCellPresence );
			clusterBlockCellPresence -= 1ll << cellPos;

			cluster->cells[cellID].sumR = clusterBlockCellR[cellPos];
			cluster->cells[cellID].sumG = clusterBlockCellG[cellPos];
			cluster->cells[cellID].sumB = clusterBlockCellB[cellPos];

			cluster->cells[cellID].minR = clusterBlockCellMinR[cellPos];
			cluster->cells[cellID].minG = clusterBlockCellMinG[cellPos];
			cluster->cells[cellID].minB = clusterBlockCellMinB[cellPos];
			
			cluster->cells[cellID].maxR = clusterBlockCellMaxR[cellPos];
			cluster->cells[cellID].maxG = clusterBlockCellMaxG[cellPos];
			cluster->cells[cellID].maxB = clusterBlockCellMaxB[cellPos];
			
			cluster->cells[cellID].count = clusterBlockCellCount[cellPos];

			++cellID;
		}
		while ( clusterBlockCellPresence != 0 );
	}

	return BLOCK_CLUSTER_LINK_RESULT_EQUAL;
}

// https://en.wikipedia.org/wiki/Hilbert_curve

void rot( int n, int *x, int *y, int rx, int ry )
{
	if (ry == 0) {
		if (rx == 1) {
			*x = n-1 - *x;
			*y = n-1 - *y;
		}

		//Swap x and y
		int t  = *x;
		*x = *y;
		*y = t;
	}
}

static void d2xy( int n, int d, int *x, int *y )
{
	int rx, ry, s, t=d;
	*x = *y = 0;
	for (s=1; s<n; s*=2) {
		rx = 1 & (t/2);
		ry = 1 & (t ^ rx);
		rot(s, x, y, rx, ry);
		*x += s * rx;
		*y += s * ry;
		t /= 4;
	}
}

static void RawFrame_CompressedBlockDataChunk_Job( void* jobPointer, u32 threadID, u32 chunkCount )
{
	BlockDataChunkJob_s* job = (BlockDataChunkJob_s*)jobPointer;

	for ( u32 chunkRank = 0; chunkRank < chunkCount; ++chunkRank )
	{
		ScopedStack lz4ScopedStack( &job->stack );

		BlockDataChunk_s* chunk = job->firstChunk + chunkRank;

		const u32 chunkLZ4MaxSize = LZ4_compressBound( chunk->decompressedSize );
		u8* chunckLZ4 = (u8*)job->stack.alloc( chunkLZ4MaxSize );
		chunk->compressedSize = LZ4_compress_HC( (char*)chunk->decompressedRGBA, (char*)chunckLZ4, chunk->decompressedSize, chunkLZ4MaxSize, ENCODER_LZ4_COMPRESSION_LEVEL );
		if ( chunk->compressedSize == 0 )
		{
			job->success = false;
			return;
		}
		const u32 compressedBufferOffset = Atomic_Add( job->compressedBufferSize, chunk->compressedSize );
		V6_ASSERT( compressedBufferOffset + chunk->compressedSize <= job->compressedBufferMaxSize );
		chunk->compressedRGBA = job->compressedBuffer + compressedBufferOffset;
		memcpy( chunk->compressedRGBA, chunckLZ4, chunk->compressedSize );
	}

	job->success = true;
}

static void ContextStream_PostInitDesc( ContextStream_s* contextStream )
{
	contextStream->gridMacroWidth = contextStream->desc.gridWidth >> 2;
	contextStream->gridMacroHalfWidth = contextStream->desc.gridWidth >> 3;
	if ( (contextStream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) != 0 )
		contextStream->gridCount = Codec_GetMipCount( contextStream->desc.gridScaleMin, contextStream->desc.gridScaleMax );
	else
		contextStream->gridCount = CODEC_FACE_MAX_COUNT;
}

static bool RawFrame_LoadFromFile( u32 frameRank, const char* filename, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];
	memset( frame, 0, sizeof( RawFrame_s ) );

	CFileReader fileReader;
	if ( !fileReader.Open( filename, FILE_OPEN_FLAG_UNBUFFERED ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return false;
	}

	char cacheFilename[256];
	FilePath_ChangeExtension( cacheFilename, sizeof( cacheFilename ), filename, "~v6c" );
	CFileWriter dataCacheWriter;
	if ( !dataCacheWriter.Open( cacheFilename, FILE_OPEN_FLAG_UNBUFFERED ) )
	{
		V6_ERROR( "Unable to open %s.\n", cacheFilename );
		return false;
	}

	ScopedStack scopedStack( context->stack );

	ShowProgress();

	CodecRawFrameDesc_s desc;
	CodecRawFrameData_s data;

	if ( !Codec_ReadRawFrame( &fileReader, &desc, &data, nullptr, context->stack ) )
	{
		V6_ERROR( "Unable to read %s.\n", filename );
		return false;
	}

	CodecStreamDesc_s* streamDesc = &context->stream->desc;
	if ( streamDesc->frameRate == 0 )
	{
		streamDesc->frameRate = desc.frameRate;
		streamDesc->gridWidth = desc.gridWidth;
		streamDesc->gridScaleMin = desc.gridScaleMin;
		streamDesc->gridScaleMax = desc.gridScaleMax;
		streamDesc->flags = desc.flags;
		if ( (desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) == 0 )
			streamDesc->gridOrigin = desc.gridOrigin;
		ContextStream_PostInitDesc( context->stream );
	}
	else
	{
		if ( desc.frameRate != streamDesc->frameRate )
		{
			V6_ERROR( "Incompatible frame rate.\n" );
			return false;
		}

#if 0
		if ( desc.sampleCount != streamDesc->sampleCount )
		{
			V6_ERROR( "Incompatible sample count.\n" );
			return false;
		}
#endif

		if ( desc.gridWidth != streamDesc->gridWidth )
		{
			V6_ERROR( "Incompatible grid resolution.\n" );
			return false;
		}

		if ( desc.gridScaleMin != streamDesc->gridScaleMin || desc.gridScaleMax != streamDesc->gridScaleMax )
		{
			V6_ERROR( "Incompatible grid scales.\n" );
			return false;
		}

		if ( desc.flags != streamDesc->flags )
		{
			V6_ERROR( "Incompatible flags.\n" );
			return false;
		}

		if ( (streamDesc->flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) == 0 && streamDesc->gridOrigin != desc.gridOrigin )
		{
			V6_ERROR( "Incompatible origin.\n" );
			return false;
		}
	}

	frame->gridOrigin = desc.gridOrigin;
	frame->gridYaw = desc.gridYaw;
	frame->refFrameRank = (u32)-1;

#if ENCODER_DUMP_BLOCKS == 1
	Plot_s plot;
	Plot_Create( &plot, String_Format( "d:/tmp/plot/rawframe%d", frameRank ) );
	Vec3 gridCenters[CODEC_MIP_MAX_COUNT];
	float gridScales[CODEC_MIP_MAX_COUNT];
	float cellSizes[CODEC_MIP_MAX_COUNT];
	const float invGridWidth = 1.0f / (1 << (context->stream->desc.gridMacroShift2 + 2));

	const u32 gridMacroWidth = 1 << desc.gridMacroShift2;
	const float invMacroGridWidth = 1.0f / gridMacroWidth;
	const float invMacroPeriodWidth = log2f( 1.0f + 2.0f / gridMacroWidth );
#endif // #if ENCODER_DUMP_BLOCKS == 1

	if ( (streamDesc->flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) != 0 )
	{
		float gridScale = context->stream->desc.gridScaleMin;
		for ( u32 mip = 0; mip < context->stream->gridCount; ++mip, gridScale *= 2.0f )
		{
			const Vec3i gridCoord = Codec_ComputeMacroGridCoords( &frame->gridOrigin, gridScale, context->stream->gridMacroHalfWidth );
			frame->gridMin[mip] = gridCoord - (int)context->stream->gridMacroHalfWidth;
			frame->gridMax[mip] = gridCoord + (int)context->stream->gridMacroHalfWidth;

			if ( frameRank == 0 )
			{
				context->gridMin[mip] = frame->gridMin[mip];
				context->gridMax[mip] = frame->gridMax[mip];
			}
			else
			{
				context->gridMin[mip] = Min( context->gridMin[mip], frame->gridMin[mip] );
				context->gridMax[mip] = Max( context->gridMin[mip], frame->gridMax[mip] );
			}

#if ENCODER_DUMP_BLOCKS == 1
			gridCenters[mip] = Codec_ComputeGridCenter( &frame->gridOrigin, gridScale, context->stream->gridMacroHalfWidth );
			gridScales[mip] = gridScale;
			cellSizes[mip] = 2.0f * gridScale * invGridWidth;
#endif // #if ENCODER_DUMP_BLOCKS == 1
		}
	}

	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	u32 blockPosOffsets[CODEC_RAWFRAME_BUCKET_COUNT] = {};
	u32 blockDataOffsets[CODEC_RAWFRAME_BUCKET_COUNT] = {};
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		const u32 cellCount = desc.blockCounts[bucket] * cellPerBucketCount;
		blockPosOffsets[bucket] = blockPosCount;
		blockDataOffsets[bucket] = blockDataCount;
		blockPosCount += desc.blockCounts[bucket];
		blockDataCount += cellCount;
	}

	frame->blocks = context->heap->newArray< Block_s >( blockPosCount );
	frame->blockIDs = context->heap->newArray< u32 >( blockPosCount );
	memset( frame->blocks, 0, blockPosCount * sizeof( Block_s ) );
	frame->blockCount = blockPosCount;

	const u32 chunkCount = (blockPosCount + ENCODER_BLOCK_PER_DATA_CHUNK - 1) / ENCODER_BLOCK_PER_DATA_CHUNK;
	frame->blockDataChunks = context->heap->newArray< BlockDataChunk_s >( chunkCount );
	memset( frame->blockDataChunks, 0, chunkCount * sizeof( BlockDataChunk_s ) );

	u32 blockID = 0;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank, ++blockID )
		{
#if ENCODER_DUMP_BLOCKS == 1
			Plot_NewObject( &plot, Color_Make( 255, 0, 0, 50 ) );
#endif // #if ENCODER_DUMP_BLOCKS == 1

			Block_s* block = &frame->blocks[blockID];
			block->packedBlockPos = ((u32*)data.blockPos)[blockID];
			const u32 grid = Block_GetGrid( block, context );
			if ( desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
				block->key = ComputeKeyFromMipAndBlockPos( grid, Block_GetPos( block, context ), frame->gridMin[grid] - context->gridMin[grid] );
			else
				block->key = ComputeKeyFromFaceAndBlockPos( grid, Block_GetPos( block, context ) );
			V6_ASSERT( block->key != 0 );
			block->frameRank = frameRank;
			block->bucket = bucket;
			
			frame->blockIDs[blockID] = blockID;

			++frame->blockCountPerGrid[grid];

#if ENCODER_DUMP_BLOCKS == 1
			const u32 blockDataID = blockDataOffsets[bucket] + blockRank * cellPerBucketCount;
			for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
			{
				const u32 rgba = ((u32*)data.blockData)[blockDataID + cellID];
				if ( rgba == ENCODER_EMPTY_CELL )
					break;
				
				const u32 cellPos = rgba & 0xFF;
				V6_ASSERT( cellPos < 64 );

				Vec3 pMin, pMax;
				if ( (streamDesc->flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) != 0 )
					ComputeCellCoords_Mip( &pMin, &pMax, block->packedBlockPos, context->stream->desc.gridMacroShift2, cellPos, &gridCenters[grid], cellSizes[grid], gridScales[grid] );
				else
					ComputeCellCoords_Onion( &pMin, &pMax, block->packedBlockPos, context->stream->desc.gridMacroShift2, cellPos, &desc.gridOrigin, desc.gridScaleMin, invMacroPeriodWidth, invMacroGridWidth );

				Plot_AddBox( &plot, &pMin, &pMax, false );
				Plot_AddBox( &plot, &pMin, &pMax, true );
			}
#endif // #if ENCODER_DUMP_BLOCKS == 1
		}
	}

	u32 blockOffset = 0;
	for ( u32 grid = 0; grid < CODEC_GRID_MAX_COUNT; ++grid )
	{
		frame->blockOffsetPerGrid[grid] = blockOffset;
		blockOffset += frame->blockCountPerGrid[grid];
	}

#if ENCODER_DUMP_BLOCKS == 1
	Plot_Release( &plot );
#endif // #if ENCODER_DUMP_BLOCKS == 1

	ShowProgress();

	qsort_s( frame->blockIDs, frame->blockCount, sizeof( u32 ), Block_CompareKey, frame );

	for ( u32 grid = 0; grid < context->stream->gridCount; ++grid )
		V6_ASSERT( frame->blockCountPerGrid[grid] == 0 || (
			Block_GetGrid( &frame->blocks[frame->blockIDs[frame->blockOffsetPerGrid[grid]]], context ) == grid && 
			Block_GetGrid( &frame->blocks[frame->blockIDs[frame->blockOffsetPerGrid[grid] + frame->blockCountPerGrid[grid] - 1]], context ) == grid) );

	ShowProgress();

	u32* const rgbaBuffer = context->stack->newArray< u32 >( blockDataCount );
	u32* const compressedBuffer = context->stack->newArray< u32 >( blockDataCount );
	u32 compressedBufferSize = 0;

	BlockDataChunk_s* chunk = frame->blockDataChunks;
	chunk->offsets = context->stack->newArray< u32 >( ENCODER_BLOCK_PER_DATA_CHUNK );
	u32* curRGBA = rgbaBuffer;
	chunk->decompressedRGBA = curRGBA;

	for ( u32 blockOrder = 0; blockOrder < blockPosCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		Block_s* block = &frame->blocks[blockID];
		block->thisBlockOrder = blockOrder;
		block->nextBlockOrder = ENCODER_INVALID_ID;
	
		const u32 blockRankInBucket = blockID - blockPosOffsets[block->bucket];
		const u32 cellPerBucketCount = 1 << (block->bucket + 2);
		const u32 blockDataID = blockDataOffsets[block->bucket] + blockRankInBucket * cellPerBucketCount;
	
		chunk->offsets[chunk->blockCount] = (u32)(curRGBA - chunk->decompressedRGBA);
		for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
		{
			const u32 rgba = ((u32*)(data.blockData))[blockDataID + cellID];
			V6_ASSERT( rgba == 0xFFFFFFFF || (rgba & 0xFF) < 64 );
			curRGBA[cellID] = rgba;
		}
		curRGBA += cellPerBucketCount;
		++chunk->blockCount;

		if ( blockOrder == blockPosCount-1 || chunk->blockCount == ENCODER_BLOCK_PER_DATA_CHUNK )
		{
			chunk->decompressedSize = (u32)(curRGBA - chunk->decompressedRGBA) * sizeof( u32 );
			++chunk;
			
			if ( (chunk - frame->blockDataChunks) < chunkCount )
			{
				chunk->offsets = context->stack->newArray< u32 >( ENCODER_BLOCK_PER_DATA_CHUNK );
				chunk->decompressedRGBA = curRGBA;
			}
		}
	}

	V6_ASSERT( (chunk - frame->blockDataChunks) == chunkCount );
	V6_ASSERT( (curRGBA - rgbaBuffer) == blockDataCount );

	const u32 chunkCountPerThread = chunkCount / ENCODER_THREAD_COUNT;
	const u32 chunkCountOnFirstThread = chunkCount - chunkCountPerThread * (ENCODER_THREAD_COUNT-1);

	BlockDataChunkJob_s compressJobs[ENCODER_THREAD_COUNT] = {};
	u32 chunkOffset = 0;
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		const u32 threadChunkCount = threadID == 0 ? chunkCountOnFirstThread : chunkCountPerThread;
		if ( threadChunkCount == 0 )
			break;

		BlockDataChunkJob_s* compressJob = &compressJobs[threadID];
		compressJobs[threadID].heap = context->heap;
		compressJobs[threadID].stack.Init( context->heap, ENCODER_BLOCK_PER_DATA_CHUNK * 64 * sizeof( u32 ) );
		compressJobs[threadID].firstChunk = frame->blockDataChunks + chunkOffset;
		compressJobs[threadID].compressedBuffer = (u8*)compressedBuffer;
		compressJobs[threadID].compressedBufferSize = &compressedBufferSize;
		compressJobs[threadID].compressedBufferMaxSize = blockDataCount * sizeof( u32 );

		WorkerThread_AddJob( &context->threads[threadID], RawFrame_CompressedBlockDataChunk_Job, compressJob, threadID, threadChunkCount );

		chunkOffset += threadChunkCount;
	}
	V6_ASSERT( chunkOffset == chunkCount );

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
		WorkerThread_WaitAllJobs( &context->threads[threadID] );

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		if ( !compressJobs[threadID].firstChunk )
			break;

		if ( !compressJobs[threadID].success )
		{
			V6_ERROR( "LZ4 compression failed.\n" );
			return false;
		}
	}
	
	ShowProgress();

	u32 maxDecompressedSize = 0;
	u32 totalDecompressedSize = 0;
	u32 totalCompressedSize = 0;
	for ( u32 chunkID = 0; chunkID < chunkCount; ++chunkID )
	{
		BlockDataChunk_s* chunk = &frame->blockDataChunks[chunkID];
		BlockDataChunk_Write( &dataCacheWriter, chunk, context );

		const u32 offsetSize = ENCODER_BLOCK_PER_DATA_CHUNK * sizeof( u32 );
		const u32 decompressedSize = offsetSize + chunk->decompressedSize;
		const u32 compressedSize = offsetSize + chunk->compressedSize;

		maxDecompressedSize = Max( maxDecompressedSize, decompressedSize );
		totalDecompressedSize += decompressedSize;
		totalCompressedSize += compressedSize;
	}
#if 0
	V6_MSG( "Data compression: compressed %d KB, decompressed %d KB (%.1f%%)\n",  DivKB( totalCompressedSize ), DivKB( totalDecompressedSize ), totalCompressedSize * 100.0f / totalDecompressedSize );
#endif

	dataCacheWriter.Close();

	new (&frame->cacheFileReader) CFileReader();
	if ( !frame->cacheFileReader.Open( cacheFilename, FILE_OPEN_FLAG_UNBUFFERED ) )
	{
		V6_ERROR( "Unable to open %s.\n", cacheFilename );
		return false;
	}
	
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		RawFrameBlockCache_s* cache = &frame->threadCaches[threadID];
		cache->lastBlockOrder = ENCODER_INVALID_ID;
		cache->lastBlockDataChunkID = ENCODER_INVALID_ID;
		cache->chunkBuffer = (u8*)context->heap->alloc( maxDecompressedSize );
	}

	HideProgress();

	return true;
}

static void RawFrame_Release( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	context->heap->free( frame->blocks );
	context->heap->free( frame->blockIDs );

	context->heap->free( frame->blockDataChunks );

	frame->cacheFileReader.Close();
	FileSystem_DeleteFile( frame->cacheFileReader.GetFilename() );

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
		context->heap->free( frame->threadCaches[threadID].chunkBuffer );
}

static u32 RawFrame_LinkBlocks( u32 frameRank, Context_s* context )
{
	V6_ASSERT( frameRank+1 < context->frameCount );
	RawFrame_s* curFrame = &context->frames[frameRank];
	RawFrame_s* nextFrame = &context->frames[frameRank+1];

	u32 linkCount = 0;

	ScopedStack scopedStack( context->stack );

	for ( u32 grid = 0; grid < context->stream->gridCount; ++grid )
	{
		if ( context->stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		{
			bool overlap = true; 
			for ( u32 axis = 0; axis < 3; ++axis )
			{
				if ( curFrame->gridMin[grid][axis] >= nextFrame->gridMax[grid][axis] || nextFrame->gridMin[grid][axis] >= curFrame->gridMax[grid][axis] )
				{
					overlap = false;
					break;
				}
			}

			if ( !overlap )
				continue;
		}

		u32 curBlockRank = 0;
		u32 nextBlockRank = 0;
		while ( curBlockRank < curFrame->blockCountPerGrid[grid] && nextBlockRank < nextFrame->blockCountPerGrid[grid] )
		{
			const u32 curBlockOrder = curFrame->blockOffsetPerGrid[grid] + curBlockRank;
			const u32 nextBlockOrder = nextFrame->blockOffsetPerGrid[grid] + nextBlockRank;
			const u32 curBlockID = curFrame->blockIDs[curBlockOrder];
			const u32 nextBlockID = nextFrame->blockIDs[nextBlockOrder];
			Block_s* curBlock = &curFrame->blocks[curBlockID];
			Block_s* nextBlock = &nextFrame->blocks[nextBlockID];
			V6_ASSERT( Block_GetGrid( curBlock, context ) == grid );
			V6_ASSERT( Block_GetGrid( nextBlock, context ) == grid );
			if ( curBlock->key == nextBlock->key )
			{
				curBlock->nextBlockOrder = nextBlockOrder;
				nextBlock->linked = true;
				++linkCount;
				++curBlockRank;
				++nextBlockRank;
			}
			else if ( curBlock->key < nextBlock->key )
			{
				++curBlockRank;
			}
			else
			{
				++nextBlockRank;
			}
		}
	}

	return linkCount;
}

static bool RawFrame_IsRefFrame( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];
	return frame->refFrameRank == (u32)-1;
}

static void RawFrame_Skip( u32 frameRank, u32 refFrameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	for ( u32 blockOrder = 0; blockOrder < frame->blockCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		Block_s* block = &frame->blocks[blockID];

		if ( block->linked )
			continue;

		if ( block->nextBlockOrder == ENCODER_INVALID_ID )
			continue;

		Block_GetLinkedBlock( block, context )->linked = false;
	}

	frame->refFrameRank = refFrameRank;
}

static void RawFrame_Merge_Job( void* mergeJobPointer, u32 threadID, u32 blockCount )
{
	RawFrameMergeJob_s* mergeJob = (RawFrameMergeJob_s*)mergeJobPointer;

	Context_s* context = mergeJob->context;

	const RawFrame_s* frame = &context->frames[mergeJob->frameRank];
	
	for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
	{
		const u32 blockID = mergeJob->blockIDs[blockRank];
		Block_s* block = &frame->blocks[blockID];

		if ( block->GetEncodedBlock() != nullptr )
			continue;

		BlockCluster_s* blockCluster = block->GetCluster();
		if ( !block->linked )
		{
			blockCluster = BlockCluster_Create( blockCluster, block, context, threadID );
			block->SetCluster( blockCluster );
		}

		V6_ASSERT( blockCluster != nullptr );

		if ( block->nextBlockOrder == ENCODER_INVALID_ID )
		{
			BlockCluster_ResolveColors( blockCluster, block, context, threadID );
		}
		else
		{
			BlockCluster_s transientBlockCluster;
			BlockCluster_Copy( &transientBlockCluster, blockCluster );

			Block_s* linkedBlock = Block_GetLinkedBlock( block, context );
			const BlockClusterLinkResult_e linkResult = BlockCluster_LinkColors( &transientBlockCluster, linkedBlock, context, threadID );
			if ( linkResult == BLOCK_CLUSTER_LINK_RESULT_EQUAL )
			{
				BlockCluster_Copy( blockCluster, &transientBlockCluster );
				linkedBlock->SetCluster( blockCluster );
			}
			else
			{
				Block_DetachLinkedBlock( block, context );
				if ( !block->linked || linkResult == BLOCK_CLUSTER_LINK_RESULT_NOT_EQUAL )
				{
					BlockCluster_ResolveColors( blockCluster, block, context, threadID );
				}
				else
				{
					V6_ASSERT( linkResult == BLOCK_CLUSTER_LINK_RESULT_NOT_IN_MIN_MAX );
					Atomic_Inc( &context->unresolvedBlockPerSequence );
				}
			}
		}
	}
}

static void RawFrame_Merge( u32 frameRank, Context_s* context )
{
	ScopedStack scopedStack( context->stack );

	const RawFrame_s* frame = &context->frames[frameRank];

	RawFrameMergeJob_s mergeJobs[ENCODER_THREAD_COUNT] = {};
	
	const u32 blockCountPerThread = frame->blockCount / ENCODER_THREAD_COUNT;
	const u32 blockCountOnFirstThread = frame->blockCount - blockCountPerThread * (ENCODER_THREAD_COUNT-1);
	const u32* threadBlockIDs = frame->blockIDs;
	const u32* const endBlockIDs = frame->blockIDs + frame->blockCount;
	
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		const u32 blockCount = threadID == 0 ? blockCountOnFirstThread : blockCountPerThread;
		if ( blockCount == 0 )
			break;

		RawFrameMergeJob_s* mergeJob = &mergeJobs[threadID];
		mergeJobs[threadID].context = context;
		mergeJobs[threadID].frameRank = frameRank;
		mergeJobs[threadID].blockIDs = threadBlockIDs;
		threadBlockIDs += blockCount;

		WorkerThread_AddJob( &context->threads[threadID], RawFrame_Merge_Job, mergeJob, threadID, blockCount );
	}
	V6_ASSERT( threadBlockIDs == endBlockIDs );

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
		WorkerThread_WaitAllJobs( &context->threads[threadID] );
}

static void RawFrame_SortByRange( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	u32 rootCount = 0;
	for ( u32 blockOrder = 0; blockOrder < frame->blockCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		Block_s* block = &frame->blocks[blockID];
		if ( block->linked )
			continue;

		const BlockClusterEncoded_s* encodedCluster = block->GetEncodedCluster();
		V6_ASSERT( encodedCluster != nullptr );
		block->sharedFrameCount = encodedCluster->sharedFrameCount;

		frame->blockIDs[rootCount] = blockID;
		++rootCount;
	}

	qsort_s( frame->blockIDs, rootCount, sizeof( u32 ), Block_CompareBySharedFrameCountThenByKey, frame );

	for ( u32 blockOrder = 0; blockOrder < rootCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		const Block_s* block = &frame->blocks[blockID];
		const u32 sharedFrameRank = block->sharedFrameCount;
		++frame->shareds[sharedFrameRank].blockCountPerGrid[Block_GetGrid( block, context )];
	}

	u32 blockOffset = 0;
	for ( u32 sharedFrameRank = 0; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
	{
		const u32 newBlockMask = sharedFrameRank == 0 ? (1 << 24) : 0;

		RawFrame_s::Shared_s* shared = &frame->shareds[sharedFrameRank];
		for ( u32 grid = 0; grid < context->stream->gridCount; ++grid )
		{
			if ( shared->blockCountPerGrid[grid] )
			{
				V6_ASSERT( context->rangeDefCount < CODEC_RANGE_MAX_COUNT );

				const u32 rangeID = context->rangeDefCount;
				CodecRange_s* range = &context->rangeDefs[rangeID];
				V6_ASSERT( frameRank <= 0x7F);
				V6_ASSERT( grid <= 0xF );
				V6_ASSERT( shared->blockCountPerGrid[grid] <= 0xFFFFF );
				range->frameRank7_newBlock1_grid4_blockCount20 = (frameRank << 25) | newBlockMask | (grid << 20) | shared->blockCountPerGrid[grid];

				shared->rangeIDs[grid] = rangeID;
				++context->rangeDefCount;

				//V6_MSG( "F%02d: shared %d, grid %d, blocks %8d.\n", frameRank, sharedFrameRank, grid, shared->blockCountPerGrid[grid] );
			}
			else
			{
				shared->rangeIDs[grid] = ENCODER_EMPTY_RANGE;
			}
			shared->blockCount += shared->blockCountPerGrid[grid];
			shared->blockOffsetPerGrid[grid] = blockOffset;
			blockOffset += shared->blockCountPerGrid[grid];
		}

		frame->sharedBlockCount += shared->blockCount;
	}
}

static void RawFrame_UpdateLimits( u32 frameRank, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, Context_s* context )
{
	V6_ASSERT( RawFrame_IsRefFrame( frameRank, context ) );

	u32 frameUniqueBlockCount = 0;
	u32 frameBlockRangeCount = 0;
	u32 frameBlockGroupCount = 0;

	u16* rangeIDs = data->rangeIDs;
		
	for ( u32 rangeID = 0; rangeID < context->rangeDefCount; ++rangeID )
	{
		const CodecRange_s* codecRange = &context->rangeDefs[rangeID];
			
		const u32 rangeFrameRank = codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 25;
		if ( rangeFrameRank != frameRank )
			continue;
			
		const u32 blockCount = codecRange->frameRank7_newBlock1_grid4_blockCount20 & 0xFFFFF;
		frameUniqueBlockCount += blockCount;
	}

	for ( u32 rangeRank = 0; rangeRank < desc->blockRangeCount; ++rangeRank )
	{
		const u32 rangeID = rangeIDs[rangeRank];
		const u32 blockCount = context->rangeDefs[rangeID].frameRank7_newBlock1_grid4_blockCount20 & 0xFFFFF;
		frameBlockGroupCount += (blockCount + CODEC_BLOCK_THREAD_GROUP_SIZE - 1) / CODEC_BLOCK_THREAD_GROUP_SIZE;
	}

	rangeIDs += desc->blockRangeCount;
	frameBlockRangeCount += desc->blockRangeCount;

	context->stream->desc.maxBlockRangeCountPerFrame = Max( context->stream->desc.maxBlockRangeCountPerFrame, frameBlockRangeCount );
	context->stream->desc.maxBlockGroupCountPerFrame = Max( context->stream->desc.maxBlockGroupCountPerFrame, frameBlockGroupCount );

	context->blockCountPerSequence += frameUniqueBlockCount;
	V6_ASSERT( context->blockCountPerSequence <= CODEC_BLOCK_MAX_COUNT_PER_SEQUENCE );
}

static u32 RawFrame_WriteBlocks( u32 frameRank, IStreamWriter* blockPosWriter, IStreamWriter* blockDataWriters[7], Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameRank];

	const u32 emptyRGBA = ENCODER_EMPTY_CELL;

	if ( frame->sharedBlockCount == 0 )
		return 0;

	for ( u32 sharedFrameRank = 0; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
	{
		const RawFrame_s::Shared_s* shared = &frame->shareds[sharedFrameRank];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 grid = 0; grid < context->stream->gridCount; ++grid )
		{
			for ( u32 blockRank = 0; blockRank < shared->blockCountPerGrid[grid]; ++blockRank )
			{
				const u32 blockOrder = shared->blockOffsetPerGrid[grid] + blockRank;
				const u32 blockID = frame->blockIDs[blockOrder];
				const Block_s* block = &frame->blocks[blockID];
				V6_ASSERT( Block_GetGrid( block, context ) == grid );
				blockPosWriter->Write( &block->packedBlockPos, ToX64( sizeof( u32 ) ) );
				const EncodedBlockEx_s* encodedBlock = block->GetEncodedBlock();
				V6_ASSERT( encodedBlock != nullptr );
				const u32 cellPresence0 = (encodedBlock->cellPresence >>  0) & 0xFFFFFFFF;
				const u32 cellPresence1 = (encodedBlock->cellPresence >> 32) & 0xFFFFFFFF;
				const u32 cellColorIndices0 = (encodedBlock->cellColorIndices[0] >>  0) & 0xFFFFFFFF;
				const u32 cellColorIndices1 = (encodedBlock->cellColorIndices[0] >> 32) & 0xFFFFFFFF;
				const u32 cellColorIndices2 = (encodedBlock->cellColorIndices[1] >>  0) & 0xFFFFFFFF;
				const u32 cellColorIndices3 = (encodedBlock->cellColorIndices[1] >> 32) & 0xFFFFFFFF;
				blockDataWriters[0]->Write( &cellPresence0, ToX64( sizeof( u32 ) ) );
				blockDataWriters[1]->Write( &cellPresence1, ToX64( sizeof( u32 ) ) );
				blockDataWriters[2]->Write( &encodedBlock->cellEndColors, ToX64( sizeof( u32 ) ) );
				blockDataWriters[3]->Write( &cellColorIndices0, ToX64( sizeof( u32 ) ) );
				blockDataWriters[4]->Write( &cellColorIndices1, ToX64( sizeof( u32 ) ) );
				blockDataWriters[5]->Write( &cellColorIndices2, ToX64( sizeof( u32 ) ) );
				blockDataWriters[6]->Write( &cellColorIndices3, ToX64( sizeof( u32 ) ) );
			}
		}
	}

	return frame->sharedBlockCount;
}

static u32 RawFrame_WriteRangeIDs( u32 refFrameRank, u32 frameRank, IStreamWriter* streamWriter, Context_s* context )
{
	V6_ASSERT( refFrameRank <= frameRank );
	const RawFrame_s* refFrame = &context->frames[refFrameRank];

	if ( refFrame->sharedBlockCount == 0 )
		return 0;

	u32 rangeCount = 0;

	const u32 minSharedFrameRank = frameRank - refFrameRank;
	for ( u32 sharedFrameRank = minSharedFrameRank; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
	{
		const RawFrame_s::Shared_s* shared = &refFrame->shareds[sharedFrameRank];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 grid = 0; grid < context->stream->gridCount; ++grid )
		{
			if ( !shared->blockCountPerGrid[grid] )
				continue;

			const u32 rangeID = shared->rangeIDs[grid];

			V6_ASSERT( rangeID != ENCODER_EMPTY_RANGE );
			V6_ASSERT( (rangeID & 0xFFFF) == rangeID );

			streamWriter->Write( &rangeID, ToX64( sizeof( u16 ) ) );

			++rangeCount;
		}
	}

	return rangeCount;
}

static void Context_WriteSequenceHeader( IStreamWriter* streamWriter, u32 sequenceID, Context_s* context )
{
	ScopedStack scopedStack( context->stack );
	
	CBufferWriter memoryRangeDefWriter(		context->stack->alloc( MulMB(  1llu ) ),	ToX64( MulMB(  1llu ) ) );

	memoryRangeDefWriter.Write( context->rangeDefs, ToX64( context->rangeDefCount * sizeof( CodecRange_s ) ) );

	{
		CodecSequenceDesc_s desc = {};
		desc.sequenceID = sequenceID;
		desc.frameCount = context->frameCount;
		desc.rangeDefCount = context->rangeDefCount;

		CodecSequenceData_s data = {};
		data.rangeDefs = (CodecRange_s*)memoryRangeDefWriter.GetBuffer();

		Codec_WriteSequence( streamWriter, &desc, &data );
	}

#if ENCODER_DUMP_RANGES == 1
	for ( u32 rangeID = 0; rangeID < context->rangeDefCount; ++rangeID )
	{
		const CodecRange_s* codecRange = &context->rangeDefs[rangeID];
		const u32 frameRank = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 25) & 0x7F;
		const u32 newBlock = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 24) & 1;
		const u32 grid = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 20) & 0xF;
		const u32 blockCount = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 0) & 0xFFFFF;
		V6_MSG( "Range %d: frame %d, newBlock %d, grid %d, blockCount %d\n", rangeID, frameRank, newBlock, grid, blockCount );
	}
#endif // #if ENCODER_DUMP_RANGES == 1

	V6_MSG( "Header: range defs %lld KB.\n", DivKB( ToU64( memoryRangeDefWriter.GetPos() ) ) );
}

static void RawFrame_DumpRange( u32 frameRank, Context_s* context ) 
{
	if ( RawFrame_IsRefFrame( frameRank, context ) )
	{
		for ( u32 refFrameRank = 0; refFrameRank <= frameRank; ++refFrameRank )
		{
			V6_ASSERT( refFrameRank <= frameRank );
			const RawFrame_s* refFrame = &context->frames[refFrameRank];

			if ( refFrame->sharedBlockCount == 0 )
				continue;

			const u32 minSharedFrameRank = frameRank - refFrameRank;
			for ( u32 sharedFrameRank = minSharedFrameRank; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
			{
				const RawFrame_s::Shared_s* shared = &refFrame->shareds[sharedFrameRank];
				if ( shared->blockCount == 0 )
					continue;

				for ( u32 grid = 0; grid < context->stream->gridCount; ++grid )
				{
					if ( !shared->blockCountPerGrid[grid] )
						continue;

					const u32 rangeID = shared->rangeIDs[grid];

					V6_ASSERT( rangeID != ENCODER_EMPTY_RANGE );
					V6_ASSERT( (rangeID & 0xFFFF) == rangeID );

					V6_MSG( "Frame %d: range %d in ref frame %d\n", frameRank, rangeID, refFrameRank );
				}
			}
		}
	}
}

static bool RawFrame_Write( u32 frameRank, IStreamWriter* streamWriter, Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameRank];

	ScopedStack scopedStack( context->stack );

	CBufferWriter memoryBlockPosWriter(			context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellPresences0(	context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellPresences1(	context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellEndColors(		context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellColorIndices0(	context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellColorIndices1(	context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellColorIndices2(	context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );
	CBufferWriter memoryBlockCellColorIndices3(	context->stack->alloc( MulMB( 16llu ) ),	ToX64( MulMB( 16llu ) ) );

	IStreamWriter* memoryBlockDataWriters[7] = { &memoryBlockCellPresences0, &memoryBlockCellPresences1, &memoryBlockCellEndColors, &memoryBlockCellColorIndices0, &memoryBlockCellColorIndices1, &memoryBlockCellColorIndices2, &memoryBlockCellColorIndices3 };

	CBufferWriter memoryRangeIDWriter(			context->stack->alloc( MulMB(  1llu ) ),	ToX64( MulMB(   1llu ) ) );

	u32 blockCount = 0;
	u32 rangeCount = 0;

	const bool refFrame = RawFrame_IsRefFrame( frameRank, context );

	if ( refFrame )
	{
		blockCount = RawFrame_WriteBlocks( frameRank, &memoryBlockPosWriter, memoryBlockDataWriters, context );

		for ( u32 refFrameRank = 0; refFrameRank <= frameRank; ++refFrameRank )
			rangeCount += RawFrame_WriteRangeIDs( refFrameRank, frameRank, &memoryRangeIDWriter, context );
	}

	CodecFrameDesc_s frameDesc = {};
	frameDesc.gridOrigin = frame->gridOrigin;
	frameDesc.gridYaw = frame->gridYaw;
	frameDesc.frameRank = frameRank;

	if ( refFrame )
	{
		frameDesc.blockCount = blockCount;
		frameDesc.blockRangeCount = rangeCount;

		CodecFrameData_s frameData = {};
		frameData.blockPos = (u32*)memoryBlockPosWriter.GetBuffer();
		frameData.blockCellPresences0 = (u32*)memoryBlockCellPresences0.GetBuffer();
		frameData.blockCellPresences1 = (u32*)memoryBlockCellPresences1.GetBuffer();
		frameData.blockCellEndColors = (u32*)memoryBlockCellEndColors.GetBuffer();
		frameData.blockCellColorIndices0 = (u32*)memoryBlockCellColorIndices0.GetBuffer();
		frameData.blockCellColorIndices1 = (u32*)memoryBlockCellColorIndices1.GetBuffer();
		frameData.blockCellColorIndices2 = (u32*)memoryBlockCellColorIndices2.GetBuffer();
		frameData.blockCellColorIndices3 = (u32*)memoryBlockCellColorIndices3.GetBuffer();
		frameData.rangeIDs = (u16*)memoryRangeIDWriter.GetBuffer();

		if ( !Codec_WriteFrame( streamWriter, &frameDesc, &frameData, context->stack ) )
			return false;

		RawFrame_UpdateLimits( frameRank, &frameDesc, &frameData, context );
	}
	else
	{
		frameDesc.flags = CODEC_FRAME_FLAG_MOTION;

		if ( !Codec_WriteFrame( streamWriter, &frameDesc, nullptr, context->stack ) )
			return false;
	}
	
#if 0
	V6_MSG( "F%02d: blockPos %d KB, blockData %d KB, range IDs %d KB.\n", 
		frameRank,
		DivKB( memoryBlockPosWriter.GetSize() ),
		DivKB( memoryBlockDataWriter.GetSize() ),
		DivKB( memoryRangeIDWriter.GetSize() ) );
#endif

	return true;
}

static void Context_UpdateLimits( Context_s* context )
{
	V6_ASSERT( context->blockCountPerSequence == context->resolvedBlockPerSequence );
	context->stream->desc.maxBlockCountPerSequence = Max( context->stream->desc.maxBlockCountPerSequence, context->blockCountPerSequence );
}

static u32 ContextStream_EncodeSequence( IStreamWriter* streamWriter, const char* templateRawFilename, u32 sequenceID, u32 frameOffset, const u32 frameCount, u32 compressionQuality, ContextStream_s* streamContext )
{
	ScopedStack scopedStack( streamContext->stack );

	Context_s* context = streamContext->stack->newInstance< Context_s >();
	memset( context, 0, sizeof( *context ) );

	Mutex_Create( &context->mainLock );
	context->stream = streamContext;
	context->heap = streamContext->heap;
	context->stack = streamContext->stack;
	context->frames = streamContext->stack->newArray< RawFrame_s >( frameCount );
	context->frameCount = frameCount;
	context->compressionQuality = compressionQuality;
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		WorkerThread_Create( &context->threads[threadID] );
		BlockAllocator_Create( &context->threadBlockAllocators[threadID], context->heap, MulMB( 16u ) );
	}

	const u64 prevSequenceSize = ToU64( streamWriter->GetPos() );

	// Load all frames

	V6_MSG( "Loading...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank )
	{
		const u32 frameID = frameOffset + frameRank;
		char filename[256];
		sprintf_s( filename, sizeof( filename ), templateRawFilename, frameID );

		if ( !RawFrame_LoadFromFile( frameRank, filename, context ) )
		{
			for( ; frameRank > 0; --frameRank )
				RawFrame_Release( frameRank-1, context );
			context->frameCount = 0;
			goto cleanup;
		}

		V6_MSG( "F%02d: loaded %d blocks from %s.\n", frameRank, context->frames[frameRank].blockCount, filename );
	}

	V6_MSG( "Linking...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount-1; ++frameRank )
	{
		const u32 linkCount = RawFrame_LinkBlocks( frameRank, context );
		V6_MSG( "F%02d-%02d: %8d/%d, %5.1f%% shared block pos.\n", frameRank, frameRank+1, linkCount, context->frames[frameRank].blockCount, linkCount * 100.0f / context->frames[frameRank].blockCount );
	}

	{
		const float frameToWriteRatio = (float)streamContext->desc.frameRate / streamContext->desc.playRate;
		for ( u32 pass = 0; ; ++pass )
		{
			V6_ASSERT( context->unresolvedBlockPerSequence == 0 );

			V6_MSG( "Merging (pass %d)...\n", pass+1 );

			float framePart = 1.0f;
			u32 refFrameRank = (u32)-1;
			u32 prevResolvedBlockCount = context->resolvedBlockPerSequence;
			u32 prevUnresolvedBlockCount = 0;
			for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank, framePart += frameToWriteRatio )
			{
				if ( framePart + FLT_EPSILON >= 1.0f )
				{
					RawFrame_Merge( frameRank, context );
					const u32 newResolvedBlockCount = context->resolvedBlockPerSequence - prevResolvedBlockCount;
					const u32 newUnresolvedBlockCount = context->unresolvedBlockPerSequence - prevUnresolvedBlockCount;
					prevResolvedBlockCount = context->resolvedBlockPerSequence;
					prevUnresolvedBlockCount = context->unresolvedBlockPerSequence;
					if ( newUnresolvedBlockCount )
						V6_MSG( "F%02d: %8d/%d, %5.1f%% new blocks, %8d unique blocks ( %d unresolved blocks ).\n", frameRank, newResolvedBlockCount, context->frames[frameRank].blockCount, newResolvedBlockCount * 100.0f / context->frames[frameRank].blockCount, context->resolvedBlockPerSequence, newUnresolvedBlockCount );
					else
						V6_MSG( "F%02d: %8d/%d, %5.1f%% new blocks, %8d unique blocks.\n", frameRank, newResolvedBlockCount, context->frames[frameRank].blockCount, newResolvedBlockCount * 100.0f / context->frames[frameRank].blockCount, context->resolvedBlockPerSequence );
					framePart = 0.0f;
					refFrameRank = frameRank;

					if ( context->resolvedBlockPerSequence > CODEC_BLOCK_MAX_COUNT_PER_SEQUENCE )
					{
						const u32 frameDoneCount = (pass > 0) ? context->frameCount : frameRank;
						u32 newFrameCount;
						if ( frameDoneCount == 0 || context->frameCount == 1 )
						{
							newFrameCount = 0;
							V6_ERROR( "Exceeded the limit of %d unique blocks with one frame. This is not supported.\n", CODEC_BLOCK_MAX_COUNT_PER_SEQUENCE );
						}
						else
						{
							newFrameCount = Min( frameDoneCount, context->frameCount / 2 );
							V6_MSG( "Exceeded the limit of %d unique blocks with %d frames. Sequence will be split.\n", CODEC_BLOCK_MAX_COUNT_PER_SEQUENCE, context->frameCount );
						}
						for ( ; context->frameCount > 0 ; --context->frameCount )
							RawFrame_Release( context->frameCount-1, context );
						context->frameCount = newFrameCount;
						goto cleanup;
					}
				}
				else
				{
					V6_ASSERT( refFrameRank != (u32)-1 );
					RawFrame_Skip( frameRank, refFrameRank, context );
					V6_MSG( "F%02d: skipped.\n", frameRank );
				}
			}

			if ( context->unresolvedBlockPerSequence == 0 )
				break;

			context->unresolvedBlockPerSequence = 0;
		}
	}

	V6_MSG( "Sorting by ranges...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank )
	{
		if ( RawFrame_IsRefFrame( frameRank, context ) )
		{
			RawFrame_SortByRange( frameRank, context );
			V6_MSG( "F%02d: sorted.\n", frameRank );
		}
	}

	V6_MSG( "Writing...\n" );

	Context_WriteSequenceHeader( streamWriter, sequenceID, context );

	u64 prevFileSize = ToU64( streamWriter->GetPos() );
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank )
	{
#if ENCODER_SKIP_WRITING == 0
		if ( !RawFrame_Write( frameRank, streamWriter, context ) )
		{
			for( ; frameRank < context->frameCount; ++frameRank )
				RawFrame_Release( frameRank-1, context );
			context->frameCount = 0;
			goto cleanup;
		}
#endif // #if ENCODER_SKIP_WRITING == 0

#if ENCODER_DUMP_RANGES == 1
		RawFrame_DumpRange( frameRank, context );
#endif // #if ENCODER_DUMP_RANGES == 1

		RawFrame_Release( frameRank, context );
		V6_MSG( "F%02d: added %lld KB.\n", frameRank, DivKB( ToU64( streamWriter->GetPos() ) - prevFileSize ) );
		prevFileSize = ToU64( streamWriter->GetPos() );
	}

	Context_UpdateLimits( context );

	const u64 sequenceSize = ToU64( streamWriter->GetPos() )- prevSequenceSize;
	V6_PRINT( "\n" );
	V6_MSG( "Sequence %d: %lld KB, avg of %lld KB/frame\n", sequenceID, DivKB( sequenceSize ), DivKB( sequenceSize / context->frameCount ) );
	V6_PRINT( "\n" );

cleanup:

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		WorkerThread_Release( &context->threads[threadID] );
		BlockAllocator_Release( &context->threadBlockAllocators[threadID] );
	}
	Mutex_Release( &context->mainLock );

	return context->frameCount;
}

bool VideoStream_Encode( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, u32 compressionQuality, bool extend, IAllocator* heap )
{
	if ( frameCount == 0 || playRate == 0 || playRate > CODEC_FRAME_MAX_COUNT )
	{
		V6_ERROR( "Frame count out of range.\n" );
		return false;
	}

	Stack stack( heap, 700 * 1024 * 1024 );

	ContextStream_s streamContext = {};
	streamContext.heap = heap;
	streamContext.stack = &stack;

	CodecStreamDesc_s prevStreamDesc = {};

	if ( extend )
	{
		CFileReader fileReader;
		if ( !fileReader.Open( streamFilename, 0 ) )
		{
			extend = false;
		}
		else
		{
			Codec_ReadStreamDesc( &fileReader, &prevStreamDesc );
			if ( prevStreamDesc.playRate != playRate )
			{
				V6_ERROR( "Incompatible play rate.\n" );
				return false;
			}
			if ( prevStreamDesc.frameRate == 0 )
			{
				V6_ERROR( "Invalid frame rate.\n" );
				return false;
			}
			V6_MSG( "Extending existing stream which has %d frames in %d sequences.\n", prevStreamDesc.frameCount, prevStreamDesc.sequenceCount );
		}
	}

	if ( extend )
	{
		streamContext.desc = prevStreamDesc;
		ContextStream_PostInitDesc( &streamContext );
	}

	streamContext.desc.sequenceCount = 0;
	streamContext.desc.frameCount = frameCount;
	streamContext.desc.playRate = playRate;

	CFileWriter fileWriter;
	if ( !fileWriter.Open( streamFilename, extend ? FILE_OPEN_FLAG_EXTEND : 0 ) )
	{
		V6_ERROR( "Unable to open %s.\n", streamFilename );
		return false;
	}

	if ( !extend )
	{
		V6_ASSERT( ToU64( fileWriter.GetPos() ) == 0 );
		Codec_WriteStreamDesc( &fileWriter, &streamContext.desc );
	}

	for ( u32 sequenceID = prevStreamDesc.sequenceCount; frameCount > 0; ++sequenceID )
	{
		u32 sequenceFrameCount = Min( frameCount, playRate );

		for (;;)
		{
			const u32 frameEncodedCount = ContextStream_EncodeSequence( &fileWriter, templateRawFilename, sequenceID, frameOffset, sequenceFrameCount, compressionQuality, &streamContext );
			
			if ( frameEncodedCount == 0 )
				return false;

			if ( frameEncodedCount == sequenceFrameCount )
				break;

			sequenceFrameCount = frameEncodedCount;
		}

		frameOffset += sequenceFrameCount;
		frameCount -= sequenceFrameCount;
		++streamContext.desc.sequenceCount;
	}
	V6_ASSERT( frameCount == 0 );

	if ( extend )
	{
		streamContext.desc.sequenceCount += prevStreamDesc.sequenceCount;
		streamContext.desc.frameCount += prevStreamDesc.frameCount;
	}

	fileWriter.SetPos( ToX64( 0 ) );
	Codec_WriteStreamDesc( &fileWriter, &streamContext.desc );
	
	const u64 streamSize = ToU64( fileWriter.GetSize() );
	
	V6_PRINT( "\n" );
	V6_MSG( "Stream: %lld KB with %d sequences, avg of %lld KB/sequence\n", DivKB( streamSize ), streamContext.desc.sequenceCount, DivKB( streamSize / streamContext.desc.sequenceCount ) );
	if ( streamContext.desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		V6_MSG( "Moving point of view\n" );
	else
		V6_MSG( "Static point of view\n" );

	V6_PRINT( "\n" );

	return true;
}

END_V6_NAMESPACE

#endif // #if V6_UE4_PLUGIN == 0

BEGIN_V6_NAMESPACE

void VideoStream_CancelEncodingInSeparateProcess( Process_s* process )
{
	Process_Cancel( process );
}

bool VideoStream_StartEncodingInSeparateProcess( Process_s* process, const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, u32 compressionQuality, bool extend )
{
	char cmd[256];
	sprintf_s( cmd, sizeof( cmd ), "D:/dev/v6/trunk/bin/Release/v6_encoder_2015.exe -s \"%s\" -t \"%s\" -o %d -c %d -r %d -q %d %s", 
		streamFilename, 
		templateRawFilename, 
		frameOffset, 
		frameCount, 
		playRate, 
		compressionQuality,
		extend ? "-e" : "" );

	return Process_Launch( process, cmd );
}

bool VideoStream_WaitEncodingInSeparateProcess( Process_s* process )
{
	return Process_Wait( process ) == 0;
}

bool VideoStream_EncodeInSeparateProcess( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, u32 compressionQuality, bool extend )
{
	Process_s process;

	if ( !VideoStream_StartEncodingInSeparateProcess( &process, streamFilename, templateRawFilename, frameOffset, frameCount, playRate, compressionQuality, extend ) )
		return false;

	return VideoStream_WaitEncodingInSeparateProcess( &process );
}

void VideoStream_DeleteRawFrameFiles( const char* templateRawFilename, u32 frameOffset, u32 frameCount )
{
	for ( u32 frameRank = 0; frameRank < frameCount; ++frameRank )
	{
		const u32 frameID = frameOffset + frameRank;
		char filename[256];
		sprintf_s( filename, sizeof( filename ), templateRawFilename, frameID );

		FileSystem_DeleteFile( filename );
	}
}

END_V6_NAMESPACE
