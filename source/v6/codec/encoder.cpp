/*V6*/

#include <v6/core/common.h>

#include <v6/codec/compression.h>
#include <v6/codec/encoder.h>
#include <v6/codec/codec.h>
#include <v6/core/bit.h>
#include <v6/core/color.h>
#include <v6/core/image.h>
#include <v6/core/memory.h>
#include <v6/core/plot.h>
#include <v6/core/stream.h>
#include <v6/core/string.h>
#include <v6/core/thread.h>
#include <v6/core/time.h>
#include <v6/core/vec3i.h>

#define ENCODER_DEBUG					0
#define ENCODER_SKIP_WRITING			0
#define ENCODER_DUMP_RANGES				0

#define ENCODER_EMPTY_RANGE				0xFFFFFFFF
#define ENCODER_EMPTY_CELL				0xFFFFFFFF

#define ENCODER_THREAD_COUNT			8

BEGIN_V6_NAMESPACE

#define INTERLEAVE_S( S, SHIFT, OFFSET )	(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )			INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )			INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )			INTERLEAVE_S( Z, SHIFT, 2 )

struct Context_s;

struct Block_s
{
	union
	{
		Block_s*			nextFrameBlock;
		EncodedBlockEx_s*	encodedBlock;
	};
	u64			key;
	u32			mip4_none1_pos27;
	u8			frameRank;
	u8			linked;
	u8			sharedFrameCount;
	u8			bucket;
};

struct BlockCluster_s
{
	u64		cellPresence;
	Vec3u	cellRGB[CODEC_CELL_MAX_COUNT];
	u32		cellCount[CODEC_CELL_MAX_COUNT];
};

struct RawFrame_s
{
	Block_s*	blocks;
	u32*		blockIDs;
	u32			blockCount;
	u32			refFrameRank;
	Vec3		gridOrigin;
	float		gridYaw;
	Vec3i		gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i		gridMax[CODEC_MIP_MAX_COUNT];
	u32			blockCountPerMip[CODEC_MIP_MAX_COUNT];
	u32			blockOffsetPerMip[CODEC_MIP_MAX_COUNT];

	struct 
	{
		u32		blockPosOffsets[CODEC_RAWFRAME_BUCKET_COUNT];
		u32		blockDataOffsets[CODEC_RAWFRAME_BUCKET_COUNT];
		u32*	blockCellRGBA;
	} data;

	u32			sharedBlockCount;
	struct Shared_s
	{
		u32		blockCount;
		u32		rangeIDs[CODEC_MIP_MAX_COUNT];
		u32		blockCountPerMip[CODEC_MIP_MAX_COUNT];
		u32		blockOffsetPerMip[CODEC_MIP_MAX_COUNT];
	} shareds[CODEC_FRAME_MAX_COUNT];
};

struct RawFrameBlockCache_s
{
	Context_s*			context;
	BlockAllocator_s*	blockAllocator;
	u64					lastBlockID;
	u32					lastBlockCellRGBA[64];
	u64					lastBlockCellPresence;
};

struct RawFrameMergeContext_s
{
	Context_s*	context;
	u64			lastStepTime;
	u32			frameRank;
	u32			rootCount;
	u32			processedCount;
};

struct RawFrameMergeJob_s
{
	RawFrameMergeContext_s*	mergeContext;
	const u32*				blockIDs;
	RawFrameBlockCache_s	blockCache;
	
};

struct ContextStream_s
{
	IAllocator*			heap;
	Stack*				stack;
	CodecStreamDesc_s	desc;
	u32					gridMacroWidth;
	u32					gridMacroHalfWidth;
	u32					mipCount;
};

struct Context_s
{
	Mutex_s				progressLock;
	ContextStream_s*	stream;
	IAllocator*			heap;
	IStack*				stack;
	RawFrame_s*			frames;
	WorkerThread_s		threads[ENCODER_THREAD_COUNT];
	BlockAllocator_s	blockAllocators[ENCODER_THREAD_COUNT];
	Vec3i				gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i				gridMax[CODEC_MIP_MAX_COUNT];
	CodecRange_s		rangeDefs[CODEC_RANGE_MAX_COUNT];
	u32					rangeDefCount;
	u32					frameCount;
	u32					blockPosCountPerSequence;
};

static Vec3u ComputeCellCoords( u32 blockPos, u32 gridMacroShift, u32 cellPos )
{
	const u32 gridMacroMask = (1 << gridMacroShift) - 1;
	const u32 blockX = (u32)((blockPos >> (gridMacroShift * 0)) & gridMacroMask);
	const u32 blockY = (u32)((blockPos >> (gridMacroShift * 1)) & gridMacroMask);
	const u32 blockZ = (u32)((blockPos >> (gridMacroShift * 2)) & gridMacroMask);
	const u32 cellX = (u32)((cellPos >> 0) & 3);
	const u32 cellY = (u32)((cellPos >> 2) & 3);
	const u32 cellZ = (u32)((cellPos >> 4) & 3);

	return Vec3u_Make( (blockX << 2) | cellX, (blockY << 2) | cellY, (blockZ << 2) | cellZ );
}

static u64 ComputeKeyFromMipAndBlockPos( u32 mip, u32 blockPos, u32 gridMacroShift, Vec3i blockPosTranslation )
{
	V6_ASSERT( mip <= 0xF );

	const u32 gridMacroMask = (1 << gridMacroShift) - 1;
	const u64 x = (u64)((blockPos >> (gridMacroShift * 0)) & gridMacroMask) + blockPosTranslation.x;
	const u64 y = (u64)((blockPos >> (gridMacroShift * 1)) & gridMacroMask) + blockPosTranslation.y;
	const u64 z = (u64)((blockPos >> (gridMacroShift * 2)) & gridMacroMask) + blockPosTranslation.z;

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

static void Block_GetColors( const u32** cellRGBA, u64* cellPresence, const Block_s* block, RawFrameBlockCache_s* blockCache ) 
{
	const RawFrame_s* rawFrame = &blockCache->context->frames[block->frameRank];
	const u32 blockID = (u32)(block - rawFrame->blocks);
	
	if ( blockID != blockCache->lastBlockID )
	{
		const u32 blockRankInBucket = blockID - rawFrame->data.blockPosOffsets[block->bucket];
		const u32 cellPerBucketCount = 1 << (block->bucket + 2);
		const u32 blockDataID = rawFrame->data.blockDataOffsets[block->bucket] + blockRankInBucket * cellPerBucketCount;
	
		blockCache->lastBlockCellPresence = 0;

		for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
		{
			const u32 rgba = rawFrame->data.blockCellRGBA[blockDataID + cellID];
			if ( rgba == ENCODER_EMPTY_CELL )
				break;
				
			const u32 cellPos = rgba & 0xFF;
			V6_ASSERT( cellPos < 64 );

			blockCache->lastBlockCellRGBA[cellPos] = rgba;
			blockCache->lastBlockCellPresence |= 1ll << cellPos;
		}

		blockCache->lastBlockID = blockID;
	}

	*cellRGBA = blockCache->lastBlockCellRGBA;
	*cellPresence = blockCache->lastBlockCellPresence;
}

static u32 Block_GetMip( const Block_s* block )
{
	return block->mip4_none1_pos27 >> 28;
}

static u32 Block_GetPos( const Block_s* block )
{
	return block->mip4_none1_pos27 & 0x07FFFFFF;
}

static bool BlockCluster_HasSimilarColors( const BlockCluster_s* cluster, const Block_s* block, RawFrameBlockCache_s* blockCache )
{
	const u32* blockCellRGBA;
	u64 blockCellPresence;
	Block_GetColors( &blockCellRGBA, &blockCellPresence, block, blockCache );

#if ENCODER_STRICT_CELL == 1
	if ( blockCellPresence != cluster->cellPresence )
		return false;
#endif // #if ENCODER_STRICT_CELL == 1

	u64 commonPresence = blockCellPresence & cluster->cellPresence;

	if ( commonPresence == 0 )
		return false;

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( commonPresence );
		commonPresence -= 1ll << cellPos;

		const u32 refR = (blockCellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (blockCellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (blockCellRGBA[cellPos] >>  8) & 0xFF;

		const u32 avgR = cluster->cellRGB[cellPos].x / cluster->cellCount[cellPos];
		const u32 avgG = cluster->cellRGB[cellPos].y / cluster->cellCount[cellPos];
		const u32 avgB = cluster->cellRGB[cellPos].z / cluster->cellCount[cellPos];

		if ( Abs( (int)(refR - avgR) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refG - avgG) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refB - avgB) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;

	} while ( commonPresence != 0 );

	return true;
}

static bool BlockCluster_HasSimilarAverageColors( const BlockCluster_s* cluster, const Block_s* block, RawFrameBlockCache_s* blockCache )
{
	const u32* blockCellRGBA;
	u64 blockCellPresence;
	Block_GetColors( &blockCellRGBA, &blockCellPresence, block, blockCache );

#if ENCODER_STRICT_CELL == 1
	if ( blockCellPresence != cluster->cellPresence )
		return false;
#endif // #if ENCODER_STRICT_CELL == 1

	u64 commonPresence = blockCellPresence & cluster->cellPresence;

	if ( commonPresence == 0 )
		return false;

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( commonPresence );
		commonPresence -= 1ll << cellPos;

		const u32 refR = (blockCellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (blockCellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (blockCellRGBA[cellPos] >>  8) & 0xFF;

		const u32 avgR = (refR + cluster->cellRGB[cellPos].x) / (1 + cluster->cellCount[cellPos]);
		const u32 avgG = (refG + cluster->cellRGB[cellPos].y) / (1 + cluster->cellCount[cellPos]);
		const u32 avgB = (refB + cluster->cellRGB[cellPos].z) / (1 + cluster->cellCount[cellPos]);

		if ( Abs( (int)(refR - avgR) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refG - avgG) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refB - avgB) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;

	} while ( commonPresence != 0 );

	return true;
}

static void BlockCluster_AddColors( BlockCluster_s* cluster, const Block_s* block, RawFrameBlockCache_s* blockCache )
{
	const u32* blockCellRGBA;
	u64 blockCellPresence;
	Block_GetColors( &blockCellRGBA, &blockCellPresence, block, blockCache );

	u64 blockPresence = blockCellPresence;
	V6_ASSERT( blockPresence != 0 )

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( blockPresence );
		blockPresence -= 1ll << cellPos;

		const u32 refR = (blockCellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (blockCellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (blockCellRGBA[cellPos] >>  8) & 0xFF;

		cluster->cellRGB[cellPos].x += refR;
		cluster->cellRGB[cellPos].y += refG;
		cluster->cellRGB[cellPos].z += refB;
		++cluster->cellCount[cellPos];

	} while ( blockPresence != 0 );

	cluster->cellPresence |= blockCellPresence;
}

static EncodedBlockEx_s* BlockCluster_ResolveColors( const BlockCluster_s* cluster, Block_s* block, RawFrameBlockCache_s* blockCache )
{
	u64 cellPresence = cluster->cellPresence;
	V6_ASSERT( cellPresence != 0 )
	
	u32 blockCellRGBA[64];
	u32 blockCellCount = 0;

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( cellPresence );
		cellPresence -= 1ll << cellPos;
		
		const u32 refR = cluster->cellRGB[cellPos].x / cluster->cellCount[cellPos];
		const u32 refG = cluster->cellRGB[cellPos].y / cluster->cellCount[cellPos];
		const u32 refB = cluster->cellRGB[cellPos].z / cluster->cellCount[cellPos];

		blockCellRGBA[blockCellCount] = (refR << 24) | (refG << 16) | (refB << 8) | cellPos;
		++blockCellCount;

	} while ( cellPresence != 0 );

	EncodedBlockEx_s* encodedBlock = BlockAllocator_Add< EncodedBlockEx_s >( blockCache->blockAllocator, 1 );
	memset( encodedBlock, 0, sizeof( EncodedBlockEx_s ) );
	Block_Encode_Optimize( encodedBlock, blockCellRGBA, blockCellCount );

	return encodedBlock;
}

static u32 BlockCluster_LinkColors( BlockCluster_s* cluster, Block_s* linkedBlock, u32 clusterDiffCount, u32 sharedFrameCount, RawFrameBlockCache_s* blockCache )
{
	const u32* linkedBlockCellRGBA;
	u64 linkedBlockCellPresence;
	Block_GetColors( &linkedBlockCellRGBA, &linkedBlockCellPresence, linkedBlock, blockCache );

	clusterDiffCount += Bit_GetBitHighCount( cluster->cellPresence ^ linkedBlockCellPresence );
	if ( clusterDiffCount > CODEC_COLOR_COUNT_TOLERANCE || !BlockCluster_HasSimilarAverageColors( cluster, linkedBlock, blockCache ) )
		return sharedFrameCount;

	BlockCluster_AddColors( cluster, linkedBlock, blockCache );
	++sharedFrameCount;

	if ( !linkedBlock->nextFrameBlock )
		return sharedFrameCount;

	const BlockCluster_s prevCluster = *cluster;

	const u32 nextSharedFrameCount = BlockCluster_LinkColors( cluster, linkedBlock->nextFrameBlock, clusterDiffCount, sharedFrameCount, blockCache );
	if ( nextSharedFrameCount == sharedFrameCount )
		return sharedFrameCount;

	if ( !BlockCluster_HasSimilarColors( cluster, linkedBlock, blockCache ) )
	{
		*cluster = prevCluster;
		return sharedFrameCount;
	}

	return nextSharedFrameCount;
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

static bool RawFrame_LoadFromFile( u32 frameRank, const char* filename, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];
	memset( frame, 0, sizeof( RawFrame_s ) );

	CUnbufferedFileReader fileReader;
	if ( !fileReader.Open( filename ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return false;
	}

	ScopedStack scopedStack( context->stack );

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
		streamDesc->sampleCount = desc.sampleCount;
		streamDesc->gridMacroShift = desc.gridMacroShift;
		streamDesc->gridScaleMin = desc.gridScaleMin;
		streamDesc->gridScaleMax = desc.gridScaleMax;
		context->stream->gridMacroWidth = 1 << desc.gridMacroShift;
		context->stream->gridMacroHalfWidth = context->stream->gridMacroWidth >> 1;
		context->stream->mipCount = Codec_GetMipCount( desc.gridScaleMin, desc.gridScaleMax );
	}
	else
	{
		if ( desc.frameRate != streamDesc->frameRate )
		{
			V6_ERROR( "Incompatible frame rate.\n" );
			return false;
		}

		if ( desc.sampleCount != streamDesc->sampleCount )
		{
			V6_ERROR( "Incompatible sample count.\n" );
			return false;
		}

		if ( desc.gridMacroShift != streamDesc->gridMacroShift )
		{
			V6_ERROR( "Incompatible grid resolution.\n" );
			return false;
		}

		if ( desc.gridScaleMin != streamDesc->gridScaleMin || desc.gridScaleMax != streamDesc->gridScaleMax )
		{
			V6_ERROR( "Incompatible grid scales.\n" );
			return false;
		}
	}

	frame->gridOrigin = desc.gridOrigin;
	frame->gridYaw = desc.gridYaw;
	frame->refFrameRank = (u32)-1;

#if ENCODER_DEBUG == 1
	Plot_s plot;
	Plot_Create( &plot, String_Format( "d:/tmp/plot/rawframe%d", frameRank ) );
	Vec3 gridCenters[CODEC_MIP_MAX_COUNT];
	float gridScales[CODEC_MIP_MAX_COUNT];
	float halfCellSizes[CODEC_MIP_MAX_COUNT];
	const float invGridWidth = 1.0f / (1 << (context->stream->desc.gridMacroShift + 2));
#endif // #if ENCODER_DEBUG == 1

	float gridScale = context->stream->desc.gridScaleMin;
	for ( u32 mip = 0; mip < context->stream->mipCount; ++mip, gridScale *= 2.0f )
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

#if ENCODER_DEBUG == 1
		gridCenters[mip] = Codec_ComputeGridCenter( &frame->gridOrigin, gridScale, context->stream->gridMacroHalfWidth );
		gridScales[mip] = gridScale;
		halfCellSizes[mip] = gridScale * invGridWidth;
#endif // #if ENCODER_DEBUG == 1
	}

	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		const u32 cellCount = desc.blockCounts[bucket] * cellPerBucketCount;
		frame->data.blockPosOffsets[bucket] = blockPosCount;
		frame->data.blockDataOffsets[bucket] = blockDataCount;
		blockPosCount += desc.blockCounts[bucket];
		blockDataCount += cellCount;
	}

	frame->blocks = context->heap->newArray< Block_s >( blockPosCount );
	frame->blockIDs = context->heap->newArray< u32 >( blockPosCount );
	memset( frame->blocks, 0, blockPosCount * sizeof( Block_s ) );
	frame->blockCount = blockPosCount;

	frame->data.blockCellRGBA = context->heap->newArray< u32 >( blockDataCount );
	memcpy( frame->data.blockCellRGBA, data.blockData, blockDataCount * sizeof( u32 ) );

	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank )
		{
#if ENCODER_DEBUG == 1
			Plot_NewObject( &plot, Color_Make( 255, 0, 0, 50 ) );
#endif // #if ENCODER_DEBUG == 1

			const u32 blockPosID = frame->data.blockPosOffsets[bucket] + blockRank;

			Block_s* block = &frame->blocks[blockPosID];
			const u32 mip4_none1_pos27 = ((u32*)data.blockPos)[blockPosID];
			V6_ASSERT( ((mip4_none1_pos27 >> 27) & 1) == 0 );
			block->mip4_none1_pos27 = mip4_none1_pos27;
			block->frameRank = frameRank;
			block->bucket = bucket;

#if ENCODER_DEBUG == 1
			const u32 blockDataID = frame->data.blockDataOffsets[bucket] + blockRank * cellPerBucketCount;
			for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
			{
				const u32 rgba = ((u32*)data.blockData)[blockDataID + cellID];
				if ( rgba == ENCODER_EMPTY_CELL )
					break;
				
				const u32 cellPos = rgba & 0xFF;
				V6_ASSERT( cellPos < 64 );

				const Vec3u cellCoords = ComputeCellCoords( block->pos, context->stream->desc.gridMacroShift, cellPos );
				Vec3 pMin;
				pMin.x = gridCenters[block->mip].x + (cellCoords.x * halfCellSizes[block->mip] * 2.0f ) - gridScales[block->mip];
				pMin.y = gridCenters[block->mip].y + (cellCoords.y * halfCellSizes[block->mip] * 2.0f ) - gridScales[block->mip];
				pMin.z = gridCenters[block->mip].z + (cellCoords.z * halfCellSizes[block->mip] * 2.0f ) - gridScales[block->mip];
				const Vec3 pMax = pMin + halfCellSizes[block->mip] * 2.0f;
				Plot_AddBox( &plot, &pMin, &pMax, false );
				Plot_AddBox( &plot, &pMin, &pMax, true );
			}
#endif // #if ENCODER_DEBUG == 1

			++frame->blockCountPerMip[Block_GetMip( block )];
		}
	}

	u32 blockOffset = 0;
	for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
	{
		frame->blockOffsetPerMip[mip] = blockOffset;
		blockOffset += frame->blockCountPerMip[mip];
	}

#if ENCODER_DEBUG == 1
	Plot_Release( &plot );
#endif // #if ENCODER_DEBUG == 1

	return true;
}

static void RawFrame_Release( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	context->heap->free( frame->blocks );
	context->heap->free( frame->blockIDs );
	context->heap->free( frame->data.blockCellRGBA );
}

static void RawFrame_SortByKey( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	for ( u32 blockID = 0; blockID < frame->blockCount; ++blockID )
	{
		Block_s* block = &frame->blocks[blockID];
		const u32 mip = Block_GetMip( block );
		block->key = ComputeKeyFromMipAndBlockPos( mip, Block_GetPos( block ), context->stream->desc.gridMacroShift, frame->gridMin[mip] - context->gridMin[mip] );
		V6_ASSERT( block->key != 0 );
		frame->blockIDs[blockID] = blockID;
	}

	qsort_s( frame->blockIDs, frame->blockCount, sizeof( u32 ), Block_CompareKey, frame );

	for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
		V6_ASSERT( frame->blockCountPerMip[mip] == 0 || (
			Block_GetMip( &frame->blocks[frame->blockIDs[frame->blockOffsetPerMip[mip]]] ) == mip && 
			Block_GetMip( &frame->blocks[frame->blockIDs[frame->blockOffsetPerMip[mip] + frame->blockCountPerMip[mip] - 1]] ) == mip) );
}

static void Context_SortByKey_Job( void* contextPointer, u32 firstFrameRank, u32 frameCount )
{
	Context_s* context = (Context_s*)contextPointer;

	for ( u32 frameRank = firstFrameRank; frameCount; ++frameRank, --frameCount )
	{
		RawFrame_SortByKey( frameRank, context );
		{
			Mutex_Lock( &context->progressLock );
			V6_MSG( "F%02d: sorted.\n", frameRank );
			Mutex_Unlock( &context->progressLock );
		}
	}
}

static void Context_SortByKey( Context_s* context )
{
	const u32 frameCountPerThread = context->frameCount / ENCODER_THREAD_COUNT;
	const u32 frameCountOnFirstThread = context->frameCount - frameCountPerThread * (ENCODER_THREAD_COUNT-1);
	u32 firstFrameRank = 0;
	
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		const u32 frameCount = threadID == 0 ? frameCountOnFirstThread : frameCountPerThread;
		if ( frameCount == 0 )
			break;

		WorkerThread_AddJob( &context->threads[threadID], Context_SortByKey_Job, context, firstFrameRank, frameCount );

		firstFrameRank += frameCount;
	}
	V6_ASSERT( firstFrameRank == context->frameCount );

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
		WorkerThread_WaitAllJobs( &context->threads[threadID] );
}

static u32 RawFrame_LinkBlocks( u32 frameRank, Context_s* context )
{
	V6_ASSERT( frameRank+1 < context->frameCount );
	RawFrame_s* curFrame = &context->frames[frameRank];
	RawFrame_s* nextFrame = &context->frames[frameRank+1];

	u32 linkCount = 0;

	ScopedStack scopedStack( context->stack );

	for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
	{
		bool overlap = true; 
		for ( u32 axis = 0; axis < 3; ++axis )
		{
			if ( curFrame->gridMin[mip][axis] >= nextFrame->gridMax[mip][axis] || nextFrame->gridMin[mip][axis] >= curFrame->gridMax[mip][axis] )
			{
				overlap = false;
				break;
			}
		}

		if ( !overlap )
			continue;

		u32 curBlockRank = 0;
		u32 nextBlockRank = 0;
		while ( curBlockRank < curFrame->blockCountPerMip[mip] && nextBlockRank < nextFrame->blockCountPerMip[mip] )
		{
			const u32 curBlockOrder = curFrame->blockOffsetPerMip[mip] + curBlockRank;
			const u32 nextBlockOrder = nextFrame->blockOffsetPerMip[mip] + nextBlockRank;
			const u32 curBlockID = curFrame->blockIDs[curBlockOrder];
			const u32 nextBlockID = nextFrame->blockIDs[nextBlockOrder];
			Block_s* curBlock = &curFrame->blocks[curBlockID];
			Block_s* nextBlock = &nextFrame->blocks[nextBlockID];
			V6_ASSERT( Block_GetMip( curBlock ) == mip );
			V6_ASSERT( Block_GetMip( nextBlock ) == mip );
			if ( curBlock->key == nextBlock->key )
			{
				curBlock->nextFrameBlock = nextBlock;
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

		if ( !block->nextFrameBlock )
			continue;

		block->nextFrameBlock->linked = false;
	}

	frame->refFrameRank = refFrameRank;
}

static void RawFrame_Merge_SignalProgress( RawFrameMergeContext_s* mergeContext )
{
	const u32 doneCount = Atomic_Inc( &mergeContext->processedCount ) + 1;
	if ( (doneCount % 128) == 0 )
	{
		const u64 curStepTime = GetTickCount();
		if ( ConvertTicksToSeconds( curStepTime - mergeContext->lastStepTime ) > 5.0f )
		{
			Mutex_Lock( &mergeContext->context->progressLock );
			if ( ConvertTicksToSeconds( curStepTime - Atomic_Load( &mergeContext->lastStepTime ) ) > 5.0f )
			{
				V6_MSG( "...processed %.1f%% blocks\n", doneCount * 100.0f / mergeContext->rootCount );
				mergeContext->lastStepTime = curStepTime;
			}
			Mutex_Unlock( &mergeContext->context->progressLock );
		}
	}
}

static void RawFrame_Merge_Job( void* mergeJobPointer, u32 threaadID, u32 blockCount )
{
	RawFrameMergeJob_s* mergeJob = (RawFrameMergeJob_s*)mergeJobPointer;

	const RawFrame_s* frame = &mergeJob->blockCache.context->frames[mergeJob->mergeContext->frameRank];
	
	for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
	{
		const u32 blockID = mergeJob->blockIDs[blockRank];
		Block_s* block = &frame->blocks[blockID];

		BlockCluster_s cluster = {};
		BlockCluster_AddColors( &cluster, block, &mergeJob->blockCache );

		if ( block->nextFrameBlock )
		{
			u32 sharedFrameCount = BlockCluster_LinkColors( &cluster, block->nextFrameBlock, 0, 0, &mergeJob->blockCache );

			if ( sharedFrameCount && !BlockCluster_HasSimilarColors( &cluster, block, &mergeJob->blockCache ) )
				sharedFrameCount = 0;

			block->sharedFrameCount = sharedFrameCount;

			for ( Block_s* linkedBlock = block->nextFrameBlock; linkedBlock; linkedBlock = linkedBlock->nextFrameBlock, --sharedFrameCount )
			{
				if ( sharedFrameCount == 0 )
				{
					linkedBlock->linked = false;
					break;
				}
			}
			V6_ASSERT( sharedFrameCount == 0 );
		}

		block->encodedBlock = BlockCluster_ResolveColors( &cluster, block, &mergeJob->blockCache );
		
		RawFrame_Merge_SignalProgress( mergeJob->mergeContext );
	}
}

static u32 RawFrame_Merge( u32 frameRank, Context_s* context )
{
	ScopedStack scopedStack( context->stack );

	const RawFrame_s* frame = &context->frames[frameRank];

	u64 lastStepTime = GetTickCount();

	u32* rootBlockIDs = context->stack->newArray< u32 >( frame->blockCount );
	u32 rootCount = 0;
	
	for ( u32 blockOrder = 0; blockOrder < frame->blockCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		Block_s* block = &frame->blocks[blockID];

		if ( block->linked )
			continue;
		
		rootBlockIDs[rootCount] = blockID;
		++rootCount;
	}

	if ( rootCount == 0 )
		return 0;

	RawFrameMergeContext_s mergeContext = {};
	mergeContext.context = context;
	mergeContext.frameRank = frameRank;
	mergeContext.lastStepTime = GetTickCount();
	mergeContext.rootCount = rootCount;
	mergeContext.processedCount = 0;

	RawFrameMergeJob_s mergeJobs[ENCODER_THREAD_COUNT] = {};
	
	const u32 blockCountPerThread = rootCount / ENCODER_THREAD_COUNT;
	const u32 blockCountOnFirstThread = rootCount - blockCountPerThread * (ENCODER_THREAD_COUNT-1);
	const u32* threadBlockIDs = rootBlockIDs;
	const u32* const endBlockIDs = rootBlockIDs + rootCount;
	
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		const u32 blockCount = threadID == 0 ? blockCountOnFirstThread : blockCountPerThread;
		if ( blockCount == 0 )
			break;

		RawFrameMergeJob_s* mergeJob = &mergeJobs[threadID];
		mergeJobs[threadID].mergeContext = &mergeContext;
		mergeJobs[threadID].blockIDs = threadBlockIDs;
		mergeJobs[threadID].blockCache.context = context;
		mergeJobs[threadID].blockCache.blockAllocator = &context->blockAllocators[threadID];
		mergeJobs[threadID].blockCache.lastBlockID = 0xFFFFFFFF;
		threadBlockIDs += blockCount;

		WorkerThread_AddJob( &context->threads[threadID], RawFrame_Merge_Job, mergeJob, threadID, blockCount );
	}
	V6_ASSERT( threadBlockIDs == endBlockIDs );

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
		WorkerThread_WaitAllJobs( &context->threads[threadID] );

	V6_ASSERT( mergeContext.processedCount == rootCount );

	return rootCount;
}

static void RawFrame_SortByRange( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	u32 rootCount = 0;
	for ( u32 blockOrder = 0; blockOrder < frame->blockCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		const Block_s* block = &frame->blocks[blockID];
		if ( block->linked )
			continue;

		frame->blockIDs[rootCount] = blockID;
		++rootCount;
	}

	qsort_s( frame->blockIDs, rootCount, sizeof( u32 ), Block_CompareBySharedFrameCountThenByKey, frame );

	for ( u32 blockOrder = 0; blockOrder < rootCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		const Block_s* block = &frame->blocks[blockID];
		const u32 sharedFrameRank = block->sharedFrameCount;
		++frame->shareds[sharedFrameRank].blockCountPerMip[Block_GetMip( block )];
	}

	u32 blockOffset = 0;
	for ( u32 sharedFrameRank = 0; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
	{
		RawFrame_s::Shared_s* shared = &frame->shareds[sharedFrameRank];
		for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
		{
			if ( shared->blockCountPerMip[mip] )
			{
				V6_ASSERT( context->rangeDefCount < CODEC_RANGE_MAX_COUNT );

				const u32 rangeID = context->rangeDefCount;
				CodecRange_s* range = &context->rangeDefs[rangeID];
				V6_ASSERT( frameRank <= 0x7F);
				V6_ASSERT( mip <= 0xF );
				V6_ASSERT( shared->blockCountPerMip[mip] <= 0x1FFFFF );
				range->frameRank7_mip4_blockCount21 = (frameRank << 25) | (mip << 21) | shared->blockCountPerMip[mip];

				shared->rangeIDs[mip] = rangeID;
				++context->rangeDefCount;

				//V6_MSG( "F%02d: shared %d, mip %d, blocks %8d.\n", frameRank, sharedFrameRank, mip, shared->blockCountPerMip[mip] );
			}
			else
			{
				shared->rangeIDs[mip] = ENCODER_EMPTY_RANGE;
			}
			shared->blockCount += shared->blockCountPerMip[mip];
			shared->blockOffsetPerMip[mip] = blockOffset;
			blockOffset += shared->blockCountPerMip[mip];
		}

		frame->sharedBlockCount += shared->blockCount;
	}
}

static void RawFrame_UpdateLimits( u32 frameRank, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, Context_s* context )
{
	V6_ASSERT( RawFrame_IsRefFrame( frameRank, context ) );

	u32 frameUniqueBlockPosCount = 0;
	u32 frameBlockRangeCount = 0;
	u32 frameBlockGroupCount = 0;

	u16* rangeIDs = data->rangeIDs;
		
	for ( u32 rangeID = 0; rangeID < context->rangeDefCount; ++rangeID )
	{
		const CodecRange_s* codecRange = &context->rangeDefs[rangeID];
			
		const u32 rangeFrameRank = codecRange->frameRank7_mip4_blockCount21 >> 25;
		if ( rangeFrameRank != frameRank )
			continue;
			
		const u32 blockCount = codecRange->frameRank7_mip4_blockCount21 & 0x1FFFFF;
		frameUniqueBlockPosCount += blockCount;
	}

	for ( u32 rangeRank = 0; rangeRank < desc->blockRangeCount; ++rangeRank )
	{
		const u32 rangeID = rangeIDs[rangeRank];
		const u32 blockCount = context->rangeDefs[rangeID].frameRank7_mip4_blockCount21 & 0x1FFFFF;
		frameBlockGroupCount += (blockCount + CODEC_BLOCK_THREAD_GROUP_SIZE - 1) / CODEC_BLOCK_THREAD_GROUP_SIZE;
	}

	rangeIDs += desc->blockRangeCount;
	frameBlockRangeCount += desc->blockRangeCount;

	context->stream->desc.maxBlockRangeCountPerFrame = Max( context->stream->desc.maxBlockRangeCountPerFrame, frameBlockRangeCount );
	context->stream->desc.maxBlockGroupCountPerFrame = Max( context->stream->desc.maxBlockGroupCountPerFrame, frameBlockGroupCount );

	context->blockPosCountPerSequence += frameUniqueBlockPosCount;
}

static u32 RawFrame_WriteBlocks( u32 frameRank, IStreamWriter* blockPosWriter, IStreamWriter* blockDataWriters[4], Context_s* context )
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

		const u32 newMask = sharedFrameRank == 0 ? (1 << 27) : 0;

		for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
		{
			for ( u32 blockRank = 0; blockRank < shared->blockCountPerMip[mip]; ++blockRank )
			{
				const u32 blockOrder = shared->blockOffsetPerMip[mip] + blockRank;
				const u32 blockID = frame->blockIDs[blockOrder];
				const Block_s* block = &frame->blocks[blockID];
				V6_ASSERT( Block_GetMip( block ) == mip );
				const u32 mip4_new1_pos27 = block->mip4_none1_pos27 | newMask;
				blockPosWriter->Write( &mip4_new1_pos27, sizeof( u32 ) );
				V6_ASSERT( block->encodedBlock != nullptr );
				blockDataWriters[0]->Write( &block->encodedBlock->cellPresence, sizeof( u64 ) );
				blockDataWriters[1]->Write( &block->encodedBlock->cellEndColors, sizeof( u32 ) );
				blockDataWriters[2]->Write( &block->encodedBlock->cellColorIndices[0], sizeof( u64 ) );
				blockDataWriters[3]->Write( &block->encodedBlock->cellColorIndices[1], sizeof( u64 ) );
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

		for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
		{
			if ( !shared->blockCountPerMip[mip] )
				continue;

			const u32 rangeID = shared->rangeIDs[mip];

			V6_ASSERT( rangeID != ENCODER_EMPTY_RANGE );
			V6_ASSERT( (rangeID & 0xFFFF) == rangeID );

			streamWriter->Write( &rangeID, sizeof( u16 ) );

			++rangeCount;
		}
	}

	return rangeCount;
}

static void Context_WriteSequenceHeader( IStreamWriter* streamWriter, u32 sequenceID, Context_s* context )
{
	ScopedStack scopedStack( context->stack );
	
	CBufferWriter memoryRangeDefWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(  1 ) );

	memoryRangeDefWriter.Write( context->rangeDefs, context->rangeDefCount * sizeof( CodecRange_s ) );

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
		const u32 frameRank = codecRange->frameRank7_mip4_blockCount21 >> 25;
		const u32 mip = (codecRange->frameRank7_mip4_blockCount21 >> 21) & 0xF;
		const u32 blockCount = (codecRange->frameRank7_mip4_blockCount21 & 0x1FFFFF);
		V6_MSG( "Range %d: frame %d, mip %d, blockCount %d\n", rangeID, frameRank, mip, blockCount );
	}
#endif // #if ENCODER_DUMP_RANGES == 1

	V6_MSG( "Header: range defs %d KB.\n", DivKB( memoryRangeDefWriter.GetPos() ) );
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

				for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
				{
					if ( !shared->blockCountPerMip[mip] )
						continue;

					const u32 rangeID = shared->rangeIDs[mip];

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

	CBufferWriter memoryBlockPosWriter(			context->stack->alloc( MulMB(  10 ) ),	MulMB(  10 ) );
	CBufferWriter memoryBlockCellPresences(		context->stack->alloc( MulMB( 50 ) ),	MulMB( 50 ) );
	CBufferWriter memoryBlockCellEndColors(		context->stack->alloc( MulMB( 50 ) ),	MulMB( 50 ) );
	CBufferWriter memoryBlockCellColorIndices0(	context->stack->alloc( MulMB( 50 ) ),	MulMB( 50 ) );
	CBufferWriter memoryBlockCellColorIndices1(	context->stack->alloc( MulMB( 50 ) ),	MulMB( 50 ) );

	IStreamWriter* memoryBlockDataWriters[4] = { &memoryBlockCellPresences, &memoryBlockCellEndColors, &memoryBlockCellColorIndices0, &memoryBlockCellColorIndices1 };

	CBufferWriter memoryRangeIDWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(   1 ) );

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
		frameData.blockCellPresences = (u64*)memoryBlockCellPresences.GetBuffer();
		frameData.blockCellEndColors = (u32*)memoryBlockCellEndColors.GetBuffer();
		frameData.blockCellColorIndices0 = (u64*)memoryBlockCellColorIndices0.GetBuffer();
		frameData.blockCellColorIndices1 = (u64*)memoryBlockCellColorIndices1.GetBuffer();
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
	context->stream->desc.maxBlockCountPerSequence = Max( context->stream->desc.maxBlockCountPerSequence, context->blockPosCountPerSequence );
}

static bool ContextStream_EncodeSequence( IStreamWriter* streamWriter, const char* templateRawFilename, u32 sequenceID, u32 frameOffset, u32 frameCount, ContextStream_s* streamContext )
{
	bool success = false;

	ScopedStack scopedStack( streamContext->stack );

	Context_s* context = streamContext->stack->newInstance< Context_s >();
	memset( context, 0, sizeof( *context ) );

	Mutex_Create( &context->progressLock );
	context->stream = streamContext;
	context->heap = streamContext->heap;
	context->stack = streamContext->stack;
	context->frames = streamContext->stack->newArray< RawFrame_s >( frameCount );
	context->frameCount = frameCount;
	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		WorkerThread_Create( &context->threads[threadID] );
		BlockAllocator_Create( &context->blockAllocators[threadID], context->heap, MulMB( 16u ) );
	}

	const u32 prevSequenceSize = streamWriter->GetPos();

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
			goto cleanup;
		}

		V6_MSG( "F%02d: loaded %d blocks from %s.\n", frameRank, context->frames[frameRank].blockCount, filename );
	}

	V6_MSG( "Sorting by key...\n" );
	Context_SortByKey( context );

	V6_MSG( "Linking...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount-1; ++frameRank )
	{
		const u32 linkCount = RawFrame_LinkBlocks( frameRank, context );
		V6_MSG( "F%02d-%02d: %8d/%d, %5.1f%% shared block pos.\n", frameRank, frameRank+1, linkCount, context->frames[frameRank].blockCount, linkCount * 100.0f / context->frames[frameRank].blockCount );
	}

	V6_MSG( "Merging...\n" );
	const float frameToWriteRatio = (float)streamContext->desc.frameRate / streamContext->desc.playRate;
	float framePart = 1.0f;
	u32 refFrameRank = (u32)-1;
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank, framePart += frameToWriteRatio )
	{
		if ( framePart + FLT_EPSILON >= 1.0f )
		{
			const u32 rootCount = RawFrame_Merge( frameRank, context );
			V6_MSG( "F%02d: %8d/%d, %5.1f%% unique blocks.\n", frameRank, rootCount, context->frames[frameRank].blockCount, rootCount * 100.0f / context->frames[frameRank].blockCount );
			framePart = 0.0f;
			refFrameRank = frameRank;
		}
		else
		{
			V6_ASSERT( refFrameRank != (u32)-1 );
			RawFrame_Skip( frameRank, refFrameRank, context );
			V6_MSG( "F%02d: skipped.\n", frameRank );
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

	u32 prevFileSize = streamWriter->GetPos();
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank )
	{
#if ENCODER_SKIP_WRITING == 0
		if ( !RawFrame_Write( frameRank, streamWriter, context ) )
		{
			for( ; frameRank < context->frameCount; ++frameRank )
				RawFrame_Release( frameRank-1, context );
			goto cleanup;
		}
#endif // #if ENCODER_SKIP_WRITING == 0

#if ENCODER_DUMP_RANGES == 1
		RawFrame_DumpRange( frameRank, context );
#endif // #if ENCODER_DUMP_RANGES == 1

		RawFrame_Release( frameRank, context );
		V6_MSG( "F%02d: added %d KB.\n", frameRank, DivKB( streamWriter->GetPos() - prevFileSize ) );
		prevFileSize = streamWriter->GetPos();
	}

	Context_UpdateLimits( context );

	const u32 sequenceSize = streamWriter->GetPos() - prevSequenceSize;
	V6_PRINT( "\n" );
	V6_MSG( "Sequence %d: %d KB, avg of %d KB/frame\n", sequenceID, DivKB( sequenceSize ), DivKB( sequenceSize / context->frameCount ) );
	V6_PRINT( "\n" );

	success = true;

cleanup:

	for ( u32 threadID = 0; threadID < ENCODER_THREAD_COUNT; ++threadID )
	{
		WorkerThread_Release( &context->threads[threadID] );
		BlockAllocator_Release( &context->blockAllocators[threadID] );
	}
	Mutex_Release( &context->progressLock );

	return success;
}


bool VideoStream_Encode( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, bool extend, IAllocator* heap )
{
	if ( frameCount == 0 || playRate == 0 || playRate > CODEC_FRAME_MAX_COUNT )
	{
		V6_ERROR( "Frame count out of range.\n" );
		return false;
	}

	Stack stack( heap, 500 * 1024 * 1024 );

	ContextStream_s streamContext = {};
	streamContext.heap = heap;
	streamContext.stack = &stack;

	CodecStreamDesc_s prevStreamDesc = {};

	if ( extend )
	{
		CFileReader fileReader;
		if ( !fileReader.Open( streamFilename ) )
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
		streamContext.gridMacroWidth = 1 << prevStreamDesc.gridMacroShift;
		streamContext.gridMacroHalfWidth = streamContext.gridMacroWidth >> 1;
		streamContext.mipCount = Codec_GetMipCount( prevStreamDesc.gridScaleMin, prevStreamDesc.gridScaleMax );
	}

	streamContext.desc.sequenceCount = (frameCount + playRate - 1) / playRate;
	streamContext.desc.frameCount = frameCount;
	streamContext.desc.playRate = playRate;

	CFileWriter fileWriter;
	if ( !fileWriter.Open( streamFilename, extend ) )
	{
		V6_ERROR( "Unable to open %s.\n", streamFilename );
		return false;
	}

	if ( !extend )
	{
		V6_ASSERT( fileWriter.GetPos() == 0 );
		Codec_WriteStreamDesc( &fileWriter, &streamContext.desc );
	}

	for ( u32 sequenceRank = 0; sequenceRank < streamContext.desc.sequenceCount; ++sequenceRank )
	{
		const u32 sequenceID = prevStreamDesc.sequenceCount + sequenceRank;
		const u32 sequenceFrameCount = Min( frameCount, playRate );

		if ( !ContextStream_EncodeSequence( &fileWriter, templateRawFilename, sequenceID, frameOffset, sequenceFrameCount, &streamContext ) )
			return false;

		frameOffset += sequenceFrameCount;
		frameCount -= sequenceFrameCount;
	}
	V6_ASSERT( frameCount == 0 );

	if ( extend )
	{
		streamContext.desc.sequenceCount += prevStreamDesc.sequenceCount;
		streamContext.desc.frameCount += prevStreamDesc.frameCount;
	}

	fileWriter.SetPos( 0 );
	Codec_WriteStreamDesc( &fileWriter, &streamContext.desc );
	
	const u32 streamSize = fileWriter.GetSize();
	
	V6_PRINT( "\n" );
	V6_MSG( "Stream: %d KB with %d sequences, avg of %d KB/sequence\n", DivKB( streamSize ), streamContext.desc.sequenceCount, DivKB( streamSize / streamContext.desc.sequenceCount ) );
	V6_PRINT( "\n" );

	return true;
}

END_V6_NAMESPACE
