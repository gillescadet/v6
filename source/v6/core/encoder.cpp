/*V6*/
#include <v6/core/common.h>

#include <v6/core/bit.h>
#include <v6/core/compression.h>
#include <v6/core/encoder.h>
#include <v6/core/codec.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/vec3i.h>

#define ENCODER_EMPTY_RANGE				0xFFFFFFFF
#define ENCODER_EMPTY_CELL				0xFFFFFFFF

#define ENCODER_BC1_WIP					0

BEGIN_V6_CORE_NAMESPACE

#define INTERLEAVE_S( S, SHIFT, OFFSET )	(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )			INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )			INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )			INTERLEAVE_S( Z, SHIFT, 2 )

struct Block_s
{
	Block_s*	nextFrame;
	u64			key;
	u64			cellPresence;
	u32			pos;
	u32			cellRGBA[CODEC_CELL_MAX_COUNT];
	u8			bucket;
	u8			mip;
	u8			linked;
	u8			sharedFrameCount;
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
	u32			blockCount;
	u32*		blockIDs;
	Vec3		origin;
	Vec3i		gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i		gridMax[CODEC_MIP_MAX_COUNT];
	u32			blockCountPerMip[CODEC_MIP_MAX_COUNT];
	u32			blockOffsetPerMip[CODEC_MIP_MAX_COUNT];
};

struct BucketFrame_s
{	
	u32			blockCount;
	struct Shared_s
	{
		u32		blockCount;
		u32		rangeIDs[CODEC_MIP_MAX_COUNT];
		u32		blockCountPerMip[CODEC_MIP_MAX_COUNT];
		u32		blockOffsetPerMip[CODEC_MIP_MAX_COUNT];
	} shareds[CODEC_FRAME_MAX_COUNT];
};

struct Context_s
{
	IAllocator*		heap;
	IStack*			stack;
	RawFrame_s*		frames;
	BucketFrame_s*	bucketFrames[CODEC_BUCKET_COUNT];
	u32				frameCount;
	u32				sampleCount;
	u32				gridMacroShift;
	u32				gridMacroWidth;
	u32				gridMacroHalfWidth;
	float			gridScaleMin;
	float			gridScaleMax;
	u32				mipCount;
	Vec3i			gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i			gridMax[CODEC_MIP_MAX_COUNT];
	CodecRange_s	rangeDefs[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	u32				rangeDefCounts[CODEC_BUCKET_COUNT];
};

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

static u32 Block_GetBucket( u32 cellCount )
{
	V6_ASSERT( cellCount );
	u32 bit = Bit_GetFirstBitHigh( (u32)cellCount-1 );
	return Max( (bit == (u32)-1) ? 0u : bit, 1u ) - 1;
}

static int Block_CompareKey( void* framePointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	RawFrame_s* frame = (RawFrame_s*)framePointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	return frame->blocks[blockID0].key < frame->blocks[blockID1].key ? -1 : 1;
}

static int Block_CompareByBucketThenBySharedFrameCountThenByKey2( void* framePointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	RawFrame_s* frame = (RawFrame_s*)framePointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	const u32 bucket0 = frame->blocks[blockID0].bucket;
	const u32 bucket1 = frame->blocks[blockID1].bucket;

	if ( bucket0 < bucket1 )
		return -1;
	if ( bucket0 > bucket1 )
		return 1;

	if ( frame->blocks[blockID0].sharedFrameCount < frame->blocks[blockID1].sharedFrameCount )
		return -1;

	if ( frame->blocks[blockID0].sharedFrameCount > frame->blocks[blockID1].sharedFrameCount )
		return 1;

	return frame->blocks[blockID0].key < frame->blocks[blockID1].key ? -1 : 1;
}

static bool BlockCluster_HasSimilarColors( const BlockCluster_s* cluster, const Block_s* block )
{
#if ENCODER_STRICT_CELL == 1
	if ( block->cellPresence != cluster->cellPresence )
		return false;
#endif // #if ENCODER_STRICT_CELL == 1

	u64 commonPresence = block->cellPresence & cluster->cellPresence;

	if ( commonPresence == 0 )
		return false;

#if ENCODER_STRICT_BUCKET == 1
	const u64 allPresence = block->cellPresence | cluster->cellPresence;
	if ( block->bucket != Block_GetBucket( Bit_GetBitHighCount( allPresence ) ) )
		return false;
#endif // #if ENCODER_STRICT_BUCKET == 1

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( commonPresence );
		commonPresence -= 1ll << cellPos;

		const u32 refR = (block->cellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (block->cellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (block->cellRGBA[cellPos] >>  8) & 0xFF;

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

static bool BlockCluster_HasSimilarAverageColors( const BlockCluster_s* cluster, const Block_s* block )
{
#if ENCODER_STRICT_CELL == 1
	if ( block->cellPresence != cluster->cellPresence )
		return false;
#endif // #if ENCODER_STRICT_CELL == 1

	u64 commonPresence = block->cellPresence & cluster->cellPresence;

	if ( commonPresence == 0 )
		return false;

#if ENCODER_STRICT_BUCKET == 1
	const u64 allPresence = block->cellPresence | cluster->cellPresence;
	if ( block->bucket != Block_GetBucket( Bit_GetBitHighCount( allPresence ) ) )
		return false;
#endif // #if ENCODER_STRICT_BUCKET == 1

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( commonPresence );
		commonPresence -= 1ll << cellPos;

		const u32 refR = (block->cellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (block->cellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (block->cellRGBA[cellPos] >>  8) & 0xFF;

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

static void BlockCluster_AddColors( BlockCluster_s* cluster, const Block_s* block )
{
	u64 blockPresence = block->cellPresence;
	V6_ASSERT( blockPresence != 0 )

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( blockPresence );
		blockPresence -= 1ll << cellPos;

		const u32 refR = (block->cellRGBA[cellPos] >> 24) & 0xFF;
		const u32 refG = (block->cellRGBA[cellPos] >> 16) & 0xFF;
		const u32 refB = (block->cellRGBA[cellPos] >>  8) & 0xFF;

		cluster->cellRGB[cellPos].x += refR;
		cluster->cellRGB[cellPos].y += refG;
		cluster->cellRGB[cellPos].z += refB;
		++cluster->cellCount[cellPos];

	} while ( blockPresence != 0 );

	cluster->cellPresence |= block->cellPresence;
}

static void BlockCluster_ResolveColors( const BlockCluster_s* cluster, Block_s* block )
{
	u64 cellPresence = cluster->cellPresence;
	V6_ASSERT( cellPresence != 0 )
	
	block->cellPresence = cellPresence;
	u32 blockCellCount = 0;

	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( cellPresence );
		cellPresence -= 1ll << cellPos;
		
		const u32 refR = cluster->cellRGB[cellPos].x / cluster->cellCount[cellPos];
		const u32 refG = cluster->cellRGB[cellPos].y / cluster->cellCount[cellPos];
		const u32 refB = cluster->cellRGB[cellPos].z / cluster->cellCount[cellPos];

		block->cellRGBA[cellPos] = (refR << 24) | (refG << 16) | (refB << 8) | cellPos;
		++blockCellCount;

	} while ( cellPresence != 0 );

#if ENCODER_STRICT_BUCKET == 1
	V6_ASSERT( block->bucket == Block_GetBucket( blockCellCount ) );
#else
	block->bucket = Block_GetBucket( blockCellCount );
#endif
}

static u32 BlockCluster_LinkColors( BlockCluster_s* cluster, Block_s* linkedBlock, u32 clusterDiffCount, u32 sharedFrameCount )
{
	clusterDiffCount += Bit_GetBitHighCount( cluster->cellPresence ^ linkedBlock->cellPresence );
	if ( clusterDiffCount > CODEC_COLOR_COUNT_TOLERANCE || !BlockCluster_HasSimilarAverageColors( cluster, linkedBlock ) )
		return sharedFrameCount;

	BlockCluster_AddColors( cluster, linkedBlock );
	++sharedFrameCount;

	if ( !linkedBlock->nextFrame )
		return sharedFrameCount;

	const BlockCluster_s prevCluster = *cluster;

	const u32 nextSharedFrameCount = BlockCluster_LinkColors( cluster, linkedBlock->nextFrame, clusterDiffCount, sharedFrameCount );
	if ( nextSharedFrameCount == sharedFrameCount )
		return sharedFrameCount;

	if ( !BlockCluster_HasSimilarColors( cluster, linkedBlock ) )
	{
		*cluster = prevCluster;
		return sharedFrameCount;
	}

	return nextSharedFrameCount;
}


static bool RawFrame_LoadFromFile( u32 frameID, const char* filename, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];
	memset( frame, 0, sizeof( RawFrame_s ) );

	CFileReader fileReader;
	if ( !fileReader.Open( filename ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return false;
	}

	ScopedStack scopedStack( context->stack );

	CodecRawFrameDesc_s desc;
	CodecRawFrameData_s data;

	if ( !Codec_ReadRawFrame( &fileReader, &desc, &data, context->stack ) )
	{
		V6_ERROR( "Unable to read %s.\n", filename );
		return false;
	}

	if ( frameID == 0 )
	{
		context->sampleCount = desc.sampleCount;
		context->gridMacroShift = desc.gridMacroShift;
		context->gridMacroWidth = 1 << context->gridMacroShift;
		context->gridScaleMin = desc.gridScaleMin;
		context->gridScaleMax = desc.gridScaleMax;
		context->gridMacroHalfWidth = context->gridMacroWidth >> 1;
		context->mipCount = Codec_GetMipCount( desc.gridScaleMin, desc.gridScaleMax );
	}
	else
	{
		if ( desc.sampleCount != context->sampleCount )
		{
			V6_ERROR( "Incompatible sample count.\n" );
			return false;
		}

		if ( desc.gridMacroShift != context->gridMacroShift )
		{
			V6_ERROR( "Incompatible grid resolution.\n" );
			return false;
		}

		if ( desc.gridScaleMin != context->gridScaleMin || desc.gridScaleMax != context->gridScaleMax )
		{
			V6_ERROR( "Incompatible grid scales.\n" );
			return false;
		}
	}

	frame->origin = desc.origin;

	float gridScale = context->gridScaleMin;
	for ( u32 mip = 0; mip < context->mipCount; ++mip, gridScale *= 2.0f )
	{
		const Vec3i gridCoord = Codec_ComputeMacroGridCoords( &desc.origin, gridScale, context->gridMacroHalfWidth );
		frame->gridMin[mip] = gridCoord - (int)context->gridMacroHalfWidth;
		frame->gridMax[mip] = gridCoord + (int)context->gridMacroHalfWidth;

		if ( frameID == 0 )
		{
			context->gridMin[mip] = frame->gridMin[mip];
			context->gridMax[mip] = frame->gridMax[mip];
		}
		else
		{
			context->gridMin[mip] = Min( context->gridMin[mip], frame->gridMin[mip] );
			context->gridMax[mip] = Max( context->gridMin[mip], frame->gridMax[mip] );
		}
	}

	u32 blockPosOffsets[CODEC_BUCKET_COUNT];
	u32 blockDataOffsets[CODEC_BUCKET_COUNT];

	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const core::u32 cellPerBucketCount = 1 << (bucket + 2);

		const u32 cellCount = desc.blockCounts[bucket] * cellPerBucketCount;
		blockPosOffsets[bucket] = blockPosCount;
		blockDataOffsets[bucket] = blockDataCount;
		blockPosCount += desc.blockCounts[bucket];
		blockDataCount += cellCount;
	}

	frame->blocks = context->heap->newArray< Block_s >( blockPosCount );
	memset( frame->blocks, 0, blockPosCount * sizeof( Block_s ) );
	frame->blockCount = blockPosCount;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const core::u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank )
		{
			const u32 blockPosID = blockPosOffsets[bucket] + blockRank;

			Block_s* block = &frame->blocks[blockPosID];
			const u32 packedBlockPos = ((u32*)data.blockPos)[blockPosID];
			block->mip = packedBlockPos >> 28;
			block->pos = packedBlockPos & 0x0FFFFFFF;
			block->bucket = bucket;

			const u32 blockDataID = blockDataOffsets[bucket] + blockRank * cellPerBucketCount;
			for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
			{
				const u32 rgba = ((u32*)data.blockData)[blockDataID + cellID];
				const u32 cellPos = rgba & 0xFF;
				if ( cellPos != 0xFF )
				{
					block->cellPresence |= 1ll << cellPos;
					block->cellRGBA[cellPos] = rgba;
				}
			}

			++frame->blockCountPerMip[block->mip];
		}
	}

	u32 blockOffset = 0;
	for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
	{
		frame->blockOffsetPerMip[mip] = blockOffset;
		blockOffset += frame->blockCountPerMip[mip];
	}

	return true;
}

static void RawFrame_Release( u32 frameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

	context->heap->free( frame->blocks );
	context->heap->free( frame->blockIDs );
}

static void RawFrame_SortByKey( u32 frameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

	frame->blockIDs = context->heap->newArray< u32 >( frame->blockCount );
	for ( u32 blockID = 0; blockID < frame->blockCount; ++blockID )
	{
		Block_s* block = &frame->blocks[blockID];
		block->key = ComputeKeyFromMipAndBlockPos( block->mip, block->pos, context->gridMacroShift, frame->gridMin[block->mip] - context->gridMin[block->mip] );
		V6_ASSERT( block->key != 0 );
		frame->blockIDs[blockID] = blockID;
	}

	qsort_s( frame->blockIDs, frame->blockCount, sizeof( core::u32 ), Block_CompareKey, frame );

	for ( u32 mip = 0; mip < context->mipCount; ++mip )
		V6_ASSERT( frame->blockCountPerMip[mip] == 0 || (
			frame->blocks[frame->blockIDs[frame->blockOffsetPerMip[mip]]].mip == mip && 
			frame->blocks[frame->blockIDs[frame->blockOffsetPerMip[mip] + frame->blockCountPerMip[mip] - 1]].mip == mip) );
}

static u32 RawFrame_LinkBlocks( u32 frameID, Context_s* context )
{
	V6_ASSERT( frameID+1 < context->frameCount );
	RawFrame_s* curFrame = &context->frames[frameID];
	RawFrame_s* nextFrame = &context->frames[frameID+1];

	u32 linkCount = 0;

	ScopedStack scopedStack( context->stack );

	for ( u32 mip = 0; mip < context->mipCount; ++mip )
	{
		bool overlap = true; 
		for ( core::u32 axis = 0; axis < 3; ++axis )
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
			V6_ASSERT( curBlock->mip == mip );
			V6_ASSERT( nextBlock->mip == mip );
			if ( curBlock->key == nextBlock->key )
			{
#if ENCODER_STRICT_BUCKET == 1 || ENCODER_STRICT_CELL == 1
				if ( curBlock->bucket == nextBlock->bucket )
#endif // #if ENCODER_STRICT_BUCKET == 1 || ENCODER_STRICT_CELL == 1
				{
					curBlock->nextFrame = nextBlock;
					nextBlock->linked = true;
					++linkCount;
				}

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

static u32 RawFrame_Merge( u32 frameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

	u32 rootCount = 0;

	for ( u32 blockOrder = 0; blockOrder < frame->blockCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		Block_s* block = &frame->blocks[blockID];

		if ( block->linked )
			continue;

		++rootCount;

		if ( !block->nextFrame )
			continue;

		BlockCluster_s cluster = {};
		BlockCluster_AddColors( &cluster, block );

		u32 sharedFrameCount = BlockCluster_LinkColors( &cluster, block->nextFrame, 0, 0 );

		if ( sharedFrameCount )
		{
			if ( !BlockCluster_HasSimilarColors( &cluster, block ) )
			{
				sharedFrameCount = 0;
			}
			else
			{
				BlockCluster_ResolveColors( &cluster, block );
				block->sharedFrameCount = sharedFrameCount;
			}
		}
		
		for ( Block_s* linkedBlock = block->nextFrame; linkedBlock; linkedBlock = linkedBlock->nextFrame, --sharedFrameCount )
		{
			if ( sharedFrameCount == 0 )
			{
				linkedBlock->linked = false;
				break;
			}
		}
		V6_ASSERT( sharedFrameCount == 0 );
	}

	return rootCount;
}

static void RawFrame_SortByRange( u32 frameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

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

	qsort_s( frame->blockIDs, rootCount, sizeof( core::u32 ), Block_CompareByBucketThenBySharedFrameCountThenByKey2, frame );

	BucketFrame_s* bucketFrames[CODEC_BUCKET_COUNT];
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		bucketFrames[bucket] = &context->bucketFrames[bucket][frameID];
		memset( bucketFrames[bucket], 0, sizeof( BucketFrame_s ) );
	}

	for ( u32 blockOrder = 0; blockOrder < rootCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		const Block_s* block = &frame->blocks[blockID];
		const u32 bucket = block->bucket;
		const u32 sharedFrameID = block->sharedFrameCount;
		++bucketFrames[bucket]->shareds[sharedFrameID].blockCountPerMip[block->mip];
	}

	u32 blockOffset = 0;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		BucketFrame_s* bucketFrame = bucketFrames[bucket];
		for ( u32 sharedFrameID = 0; sharedFrameID < CODEC_FRAME_MAX_COUNT; ++sharedFrameID )
		{
			BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameID];

			for ( u32 mip = 0; mip < context->mipCount; ++mip )
			{
				if ( shared->blockCountPerMip[mip] )
				{
					V6_ASSERT( context->rangeDefCounts[bucket] < CODEC_RANGE_MAX_COUNT );

					const u32 rangeID = context->rangeDefCounts[bucket];
					CodecRange_s* range = &context->rangeDefs[bucket][rangeID];
					V6_ASSERT( frameID <= 0xFF);
					V6_ASSERT( mip <= 0xF );
					V6_ASSERT( shared->blockCountPerMip[mip] <= 0xFFFFF );
					range->frameID8_mip4_blockCount20 = (frameID << 24) | (mip << 20) | shared->blockCountPerMip[mip];

					shared->rangeIDs[mip] = rangeID;
					++context->rangeDefCounts[bucket];

					//V6_MSG( "F%02d: bucket %d, shared %d, mip %d, blocks %8d.\n", frameID, bucket, sharedFrameID, mip, shared->blockCountPerMip[mip] );
				}
				else
				{
					shared->rangeIDs[mip] = ENCODER_EMPTY_RANGE;
				}
				shared->blockCount += shared->blockCountPerMip[mip];
				shared->blockOffsetPerMip[mip] = blockOffset;
				blockOffset += shared->blockCountPerMip[mip];
			}

			bucketFrame->blockCount += shared->blockCount;
		}
	}
}

static u32 BucketFrame_WriteBlocks( u32 bucket, u32 frameID, IStreamWriter* blockPosWriter, IStreamWriter* blockDataWriter, Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameID];
	BucketFrame_s* bucketFrame = &context->bucketFrames[bucket][frameID];

	const u32 emptyRGBA = ENCODER_EMPTY_CELL;

	if ( bucketFrame->blockCount == 0 )
		return 0;

	const u32 perBucketCellCount = 1 << (bucket + 2);

	for ( u32 sharedFrameID = 0; sharedFrameID < CODEC_FRAME_MAX_COUNT; ++sharedFrameID )
	{
		BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameID];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 mip = 0; mip < context->mipCount; ++mip )
		{
			for ( u32 blockRank = 0; blockRank < shared->blockCountPerMip[mip]; ++blockRank )
			{
				const u32 blockOrder = shared->blockOffsetPerMip[mip] + blockRank;
				const u32 blockID = frame->blockIDs[blockOrder];
				const Block_s* block = &frame->blocks[blockID];
				V6_ASSERT( block->mip == mip );
				V6_ASSERT( block->bucket == bucket );
				const u32 packedBlockPos = (block->mip << 28) | block->pos;
				blockPosWriter->Write( &packedBlockPos, sizeof( u32 ) );
				u32 cellRGBA[CODEC_CELL_MAX_COUNT];
				u32 cellCount = 0;
				{
					u64 cellPresence = block->cellPresence;
					V6_ASSERT( cellPresence );
					do
					{
						const u32 cellPos = Bit_GetFirstBitHigh( cellPresence );
						cellPresence -= 1ll << cellPos;
						cellRGBA[cellCount] = block->cellRGBA[cellPos];
						++cellCount;
					} while ( cellPresence != 0 );
				}
#if CODEC_COLOR_COMPRESS == 1
				EncodedBlockEx_s encodedBlock;
				Block_Encode( &encodedBlock, cellRGBA, cellCount );
				blockDataWriter->Write( &encodedBlock, cellCount <= 32 ? sizeof( EncodedBlock_s ) : sizeof( EncodedBlockEx_s ) );
#else
				const u32 cellPerBucketCount = 1 << (2 + bucket);
				while ( cellCount < cellPerBucketCount )
				{
					cellRGBA[cellCount] = ENCODER_EMPTY_CELL;
					++cellCount;
				}
				blockDataWriter->Write( cellRGBA, cellCount * sizeof( u32 ) );
#endif
			}
		}
	}

	return bucketFrame->blockCount;
}

static u32 BucketFrame_WriteRangeIDs( u32 bucket, u32 refFrameID, u32 frameID, IStreamWriter* streamWriter, Context_s* context )
{
	V6_ASSERT( refFrameID <= frameID );
	const RawFrame_s* frame = &context->frames[frameID];

	BucketFrame_s* bucketFrame = &context->bucketFrames[bucket][refFrameID];

	if ( bucketFrame->blockCount == 0 )
		return 0;

	u32 rangeCount = 0;

	const u32 minSharedFrameID = frameID - refFrameID;
	for ( u32 sharedFrameID = minSharedFrameID; sharedFrameID < CODEC_FRAME_MAX_COUNT; ++sharedFrameID )
	{
		BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameID];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 mip = 0; mip < context->mipCount; ++mip )
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

static void Context_WriteSequenceHeader( IStreamWriter* streamWriter, Context_s* context )
{
	ScopedStack scopedStack( context->stack );
	
	CMemoryWriter memoryRangeDefWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(  1 ) );

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		memoryRangeDefWriter.Write( context->rangeDefs[bucket], context->rangeDefCounts[bucket] * sizeof( CodecRange_s ) );

	{
		CodecSequenceDesc_s desc = {};
		desc.frameCount = context->frameCount;
		desc.sampleCount = context->sampleCount;
		desc.gridMacroShift = context->gridMacroShift;
		desc.gridScaleMin = context->gridScaleMin;
		desc.gridScaleMax = context->gridScaleMax;
		memcpy( desc.rangeDefCounts, context->rangeDefCounts, sizeof( context->rangeDefCounts ) );

		CodecSequenceData_s data = {};
		data.rangeDefs = (CodecRange_s*)memoryRangeDefWriter.GetBuffer();

		Codec_WriteSequence( streamWriter, &desc, &data );
	}

	V6_MSG( "Header: range defs %d KB.\n", DivKB( memoryRangeDefWriter.GetSize() ) );
}

static void RawFrame_Write( u32 frameID, IStreamWriter* streamWriter, Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameID];

	ScopedStack scopedStack( context->stack );

	CMemoryWriter memoryBlockPosWriter(		context->stack->alloc( MulMB(  10 ) ),	MulMB(  10 ) );
	CMemoryWriter memoryBlockDataWriter(	context->stack->alloc( MulMB( 200 ) ),	MulMB( 200 ) );
	CMemoryWriter memoryRangeIDWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(   1 ) );

	u32 blockCounts[CODEC_BUCKET_COUNT] = {};
	u32 rangeCounts[CODEC_BUCKET_COUNT] = {};

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		blockCounts[bucket] = BucketFrame_WriteBlocks( bucket, frameID, &memoryBlockPosWriter, &memoryBlockDataWriter, context );

		for ( u32 refFrameID = 0; refFrameID <= frameID; ++refFrameID )
			rangeCounts[bucket] += BucketFrame_WriteRangeIDs( bucket, refFrameID, frameID, &memoryRangeIDWriter, context );
	}

	{
		CodecFrameDesc_s desc = {};
		desc.origin = frame->origin;
		desc.frameID = frameID;
		memcpy( desc.blockCounts, blockCounts, sizeof( desc.blockCounts ) );
		memcpy( desc.blockRangeCounts, rangeCounts, sizeof( desc.blockRangeCounts ) );

		CodecFrameData_s data = {};
		data.blockPos = (u32*)memoryBlockPosWriter.GetBuffer();
		data.blockData = (u32*)memoryBlockDataWriter.GetBuffer();
		data.rangeIDs = (u16*)memoryRangeIDWriter.GetBuffer();

		Codec_WriteFrame( streamWriter, &desc, &data );
	}
	
	V6_MSG( "F%02d: blockPos %d KB, blockData %d KB, range IDs %d KB.\n", 
		frameID,
		DivKB( memoryBlockPosWriter.GetSize() ),
		DivKB( memoryBlockDataWriter.GetSize() ),
		DivKB( memoryRangeIDWriter.GetSize() ) );
}

bool Sequence_Encode( const char* templateFilename, u32 fileCount, const char* streamFilename, IAllocator* heap )
{
	if ( fileCount == 0 || fileCount > CODEC_FRAME_MAX_COUNT )
	{
		V6_ERROR( "Frame count out of range.\n" );
		return false;
	}

	CFileWriter fileWriter;
	if ( !fileWriter.Open( streamFilename ) )
	{
		V6_ERROR( "Unable to open %s.\n", streamFilename );
		return false;
	}

	Stack stack( heap, 300 * 1024 * 1024 );

	Context_s* context = stack.newInstance< Context_s >();
	memset( context, 0, sizeof( *context ) );

	context->heap = heap;
	context->stack = &stack;
	context->frames = stack.newArray< RawFrame_s >( fileCount );
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		context->bucketFrames[bucket] = stack.newArray< BucketFrame_s >( fileCount );
	context->frameCount = fileCount;

	// Load all frames

	V6_MSG( "Loading...\n" );
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{
		char filename[256];
		sprintf_s( filename, sizeof( filename ), templateFilename, frameID );

		if ( !RawFrame_LoadFromFile( frameID, filename, context ) )
		{
			for( ; frameID > 0; --frameID )
				RawFrame_Release( frameID-1, context );
			return false;
		}

		V6_MSG( "F%02d: loaded %d blocks from %s.\n", frameID, context->frames[frameID].blockCount, filename );
	}

	V6_MSG( "Sorting by key...\n" );
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{
		RawFrame_SortByKey( frameID, context );
		V6_MSG( "F%02d: sorted.\n", frameID );
	}

	V6_MSG( "Linking...\n" );
	for ( u32 frameID = 0; frameID < context->frameCount-1; ++frameID )
	{
		const u32 linkCount = RawFrame_LinkBlocks( frameID, context );
		V6_MSG( "F%02d: %8d/%d, %5.1f%% shared block pos.\n", frameID, linkCount, context->frames[frameID].blockCount, linkCount * 100.0f / context->frames[frameID].blockCount );
	}

	V6_MSG( "Merging...\n" );
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{
		const u32 rootCount = RawFrame_Merge( frameID, context );
		V6_MSG( "F%02d: %8d/%d, %5.1f%% unique blocks.\n", frameID, rootCount, context->frames[frameID].blockCount, rootCount * 100.0f / context->frames[frameID].blockCount );
	}

	V6_MSG( "Sorting by ranges...\n" );
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{
		RawFrame_SortByRange( frameID, context );
		V6_MSG( "F%02d: sorted.\n", frameID );
	}

	V6_MSG( "Writing...\n" );

	Context_WriteSequenceHeader( &fileWriter, context );

	u32 prevFileSize = fileWriter.GetPos();
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{		
		RawFrame_Write( frameID, &fileWriter, context );
		RawFrame_Release( frameID, context );
		V6_MSG( "F%02d: added %d KB.\n", frameID, DivKB( fileWriter.GetPos() - prevFileSize ) );
		prevFileSize = fileWriter.GetPos();
	}

	V6_PRINT( "\n" );
	V6_MSG( "Sequence: %d KB, avg of %d KB/frame\n", DivKB( fileWriter.GetSize() ), DivKB( fileWriter.GetSize() / context->frameCount ) );

	return true;
}

END_V6_CORE_NAMESPACE
