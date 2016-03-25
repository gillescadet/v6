/*V6*/
#include <v6/core/common.h>

#include <v6/core/bit.h>
#include <v6/core/encoder.h>
#include <v6/core/codec.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/vec3i.h>

#define ENCODER_SHARED_FRAME_MAX_COUNT	256
#define ENCODER_RANGE_MAX_COUNT			65535
#define ENCODER_EMPTY_RANGE				0xFFFFFFFF
#define ENCODER_COLOR_ERROR_TOLERANCE	0x0F
#define ENCODER_EMPTY_CELL				0xFFFFFFFF

BEGIN_V6_CORE_NAMESPACE

struct Block_s
{
	u16			bucket;
	u16			mip;
	u32			pos;
	u64			key;
	u32			data[CODEC_CELL_MAX_COUNT];
	Block_s*	merged;
};

struct RawFrame_s
{
	Block_s*	blocks;
	u32			blockCount;
	u32*		blockIDs;
	Vec3i		gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i		gridMax[CODEC_MIP_MAX_COUNT];
	struct Shared_s
	{
		u32		blockCount;
		u32		blockCountPerMip[CODEC_MIP_MAX_COUNT];
		u32		blockOffsetPerMip[CODEC_MIP_MAX_COUNT];
	} shareds[ENCODER_SHARED_FRAME_MAX_COUNT];
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
	} shareds[ENCODER_SHARED_FRAME_MAX_COUNT];
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
	u32				blockGlobalCount;
	Vec3i			gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i			gridMax[CODEC_MIP_MAX_COUNT];
	CodecRange_s	ranges[CODEC_BUCKET_COUNT][ENCODER_RANGE_MAX_COUNT];
	u32				rangeCounts[CODEC_BUCKET_COUNT];
};

#define INTERLEAVE_S( S, SHIFT, OFFSET )	(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )			INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )			INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )			INTERLEAVE_S( Z, SHIFT, 2 )

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

static bool Block_HasSimilarData( const Block_s* refBlock, const Block_s* newBlock )
{
	V6_ASSERT( refBlock->mip == newBlock->mip );
	V6_ASSERT( refBlock->key == newBlock->key );

	if ( refBlock->bucket != newBlock->bucket )
		return false;

	const u32 cellCount = 1 << (refBlock->bucket + 2);
	for ( u32 cellID = 0; cellID < cellCount; ++cellID )
	{
		const u32 refData = refBlock->data[cellID];
		const u32 newData = newBlock->data[cellID];
		const u32 refCellPos = refData & 0xFF;
		const u32 newCellPos = newData & 0xFF;
		if ( refCellPos != newCellPos )
			return false;

		const u32 refR = (refData >> 24) & 0xFF;
		const u32 refG = (refData >> 16) & 0xFF;
		const u32 refB = (refData >>  8) & 0xFF;
		const u32 newR = (newData >> 24) & 0xFF;
		const u32 newG = (newData >> 16) & 0xFF;
		const u32 newB = (newData >>  8) & 0xFF;

#if 1
		if ( Abs( (int)(refR - newR) ) > ENCODER_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refG - newG) ) > ENCODER_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refB - newB) ) > ENCODER_COLOR_ERROR_TOLERANCE )
			return false;

		if ( refBlock->merged )
			return Block_HasSimilarData( refBlock->merged, newBlock );
#endif
	}

	return true;
}

static void Block_LinkData( Block_s* refBlock, Block_s* newBlock )
{
	Block_s** mergedBlock = &refBlock->merged;
	while ( *mergedBlock )
		mergedBlock = &(*mergedBlock)->merged;
	*mergedBlock = newBlock;
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

	context->stack->push();

	CodecRawFrameDesc_s desc;
	CodecRawFrameData_s data;

	if  ( !Codec_ReadRawFrame( &fileReader, &desc, &data, context->stack ) )
	{
		context->stack->pop();

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
			context->stack->pop();

			V6_ERROR( "Incompatible sample count.\n" );
			return false;
		}

		if ( desc.gridMacroShift != context->gridMacroShift )
		{
			context->stack->pop();

			V6_ERROR( "Incompatible grid resolution.\n" );
			return false;
		}

		if ( desc.gridScaleMin != context->gridScaleMin || desc.gridScaleMax != context->gridScaleMax )
		{
			context->stack->pop();

			V6_ERROR( "Incompatible grid scales.\n" );
			return false;
		}
	}

	float gridScale = context->gridScaleMin;
	for ( u32 mip = 0; mip < context->mipCount; ++mip, gridScale *= 2.0f )
	{
		const float invBlockSize = context->gridMacroHalfWidth / gridScale;
		const Vec3 gridOrg = desc.origin * invBlockSize;
		const Vec3i gridCoord = Vec3i_Make( (int)floorf( gridOrg.x ), (int)floorf( gridOrg.y ), (int)floorf( gridOrg.z ) );
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
	frame->shareds[0].blockCount = frame->blockCount;

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const core::u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank )
		{
			const u32 blockPosID = blockPosOffsets[bucket] + blockRank;

			Block_s* block = &frame->blocks[blockPosID];
			const u32 packedBlockPos = ((u32*)data.blockPos)[blockPosID];
			block->bucket = bucket;
			block->mip = packedBlockPos >> 28;
			block->pos = packedBlockPos & 0x0FFFFFFF;

			const u32 blockDataID = blockDataOffsets[bucket] + blockRank * cellPerBucketCount;
			for ( u32 cellID = 0; cellID < CODEC_CELL_MAX_COUNT; ++cellID )
				block->data[cellID] = (cellID < cellPerBucketCount) ? ((u32*)data.blockData)[blockDataID+cellID] : ENCODER_EMPTY_CELL;

			++frame->shareds[0].blockCountPerMip[block->mip];
		}
	}

	u32 blockOffset = 0;
	for ( u32 mip = 0; mip < context->mipCount; ++mip )
	{
		frame->shareds[0].blockOffsetPerMip[mip] = blockOffset;
		blockOffset += frame->shareds[0].blockCountPerMip[mip];
	}

	context->stack->pop();

	return true;
}

static bool RawFrame_CheckOrder( u32 frameID, u32 sharedFrameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

	RawFrame_s::Shared_s* shared = &frame->shareds[sharedFrameID];

	u64 prevKey = 0;
	for ( u32 blockRank = 0; blockRank < shared->blockCount; ++blockRank )
	{
		const u32 blockOrder = shared->blockOffsetPerMip[0] + blockRank;
		const u32 blockID = frame->blockIDs[blockOrder];
		Block_s* block = &frame->blocks[blockID];
		if ( prevKey >= block->key )
			return false;

		prevKey = block->key;
	}

	return true;
}

static void RawFrame_SortByKey( u32 frameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

	frame->blockIDs = context->heap->newArray< u32 >( frame->blockCount );
	for ( u32 blockID = 0; blockID < frame->blockCount; ++blockID )
	{
		Block_s* block = &frame->blocks[blockID];
		block->key = ComputeKeyFromMipAndBlockPos( block->mip, block->pos, context->gridMacroShift, frame->gridMin[block->mip] - context->gridMin[block->mip] );
		frame->blockIDs[blockID] = blockID;
	}

	qsort_s( frame->blockIDs, frame->blockCount, sizeof( core::u32 ), Block_CompareKey, frame );

	for ( u32 mip = 0; mip < context->mipCount; ++mip )
		V6_ASSERT( frame->shareds[0].blockCountPerMip[mip] == 0 || (
			frame->blocks[frame->blockIDs[frame->shareds[0].blockOffsetPerMip[mip]]].mip == mip && 
			frame->blocks[frame->blockIDs[frame->shareds[0].blockOffsetPerMip[mip] + frame->shareds[0].blockCountPerMip[mip] - 1]].mip == mip) );
}

static void RawFrame_TrimShared( u32 frameID, u32 sharedFrameID, BitSet_s* sharedBlockBitSet, Context_s* context )
{
	V6_ASSERT( sharedFrameID+1 < ENCODER_SHARED_FRAME_MAX_COUNT );

	RawFrame_s* frame = &context->frames[frameID];

	RawFrame_s::Shared_s* unique = &frame->shareds[sharedFrameID];
	RawFrame_s::Shared_s* shared = &frame->shareds[sharedFrameID+1];
	memset( shared, 0, sizeof( *shared ) );

	context->stack->push();

	u32* uniqueBlockIDs = context->stack->newArray< u32 >( unique->blockCount );
	u32* sharedBlockIDs = context->stack->newArray< u32 >( unique->blockCount );

	u32 sharedBlockCount = 0;
	u32 uniqueBlockCount = 0;

	const u32 blockOrderMin = unique->blockOffsetPerMip[0];
	for ( u32 blockRank = 0; blockRank < unique->blockCount; ++blockRank )
	{
		const u32 blockOrder = blockOrderMin + blockRank;
		const u32 blockID = frame->blockIDs[blockOrder];
		const u32 mip = frame->blocks[blockID].mip;
		if ( BitSet_GetBit( sharedBlockBitSet, blockRank ) )
		{
			sharedBlockIDs[sharedBlockCount] = blockID;
			++sharedBlockCount;

			++shared->blockCountPerMip[mip];
			--unique->blockCountPerMip[mip];
		}
		else
		{
			uniqueBlockIDs[uniqueBlockCount] = blockID;
			++uniqueBlockCount;
		}
	}

	unique->blockCount = 0;
	for ( u32 mip = 0; mip < context->mipCount; ++mip )
	{
		unique->blockOffsetPerMip[mip] = blockOrderMin + unique->blockCount;
		unique->blockCount += unique->blockCountPerMip[mip];
	}
	V6_ASSERT( unique->blockCount == uniqueBlockCount );

	shared->blockCount = 0;
	for ( u32 mip = 0; mip < context->mipCount; ++mip )
	{
		shared->blockOffsetPerMip[mip] = blockOrderMin + unique->blockCount + shared->blockCount;
		shared->blockCount += shared->blockCountPerMip[mip];
	}
	V6_ASSERT( shared->blockCount == sharedBlockCount );

	memcpy( frame->blockIDs + unique->blockOffsetPerMip[0], uniqueBlockIDs, unique->blockCount * sizeof( u32 ) );
	memcpy( frame->blockIDs + shared->blockOffsetPerMip[0], sharedBlockIDs, shared->blockCount * sizeof( u32 ) );

	context->stack->pop();
}

static u32 RawFrame_Merge( u32 refFrameID, u32 newFrameID, Context_s* context )
{
	V6_ASSERT( refFrameID < newFrameID );
	RawFrame_s* refFrame = &context->frames[refFrameID];
	RawFrame_s* newFrame = &context->frames[newFrameID];

	u32 sharedCount = 0;

	const u32 sharedFrameID = newFrameID-refFrameID-1;
	RawFrame_s::Shared_s* refShareds = &refFrame->shareds[sharedFrameID];
	RawFrame_s::Shared_s* newShareds = &newFrame->shareds[0];

	context->stack->push();

	u32* refSharedBlockBits = context->stack->newArray< u32 >( BitSet_GetSize( refShareds->blockCount ) );
	u32* newSharedBlockBits = context->stack->newArray< u32 >( BitSet_GetSize( newShareds->blockCount ) );

	BitSet_s refSharedBlockBitSet;
	BitSet_s newSharedBlockBitSet;

	BitSet_Init( &refSharedBlockBitSet, refSharedBlockBits, refShareds->blockCount );
	BitSet_Init( &newSharedBlockBitSet, newSharedBlockBits, newShareds->blockCount );
	BitSet_Clear( &refSharedBlockBitSet );
	BitSet_Clear( &newSharedBlockBitSet );

	for ( u32 mip = 0; mip < context->mipCount; ++mip )
	{
		bool overlap = true; 
		for ( core::u32 axis = 0; axis < 3; ++axis )
		{
			if ( refFrame->gridMin[mip][axis] >= newFrame->gridMax[mip][axis] || newFrame->gridMin[mip][axis] >= refFrame->gridMax[mip][axis] )
			{
				overlap = false;
				break;
			}
		}

		if ( !overlap )
			continue;

		u32 refBlockRank = 0;
		u32 newBlockRank = 0;
		while ( refBlockRank < refShareds->blockCountPerMip[mip] && newBlockRank < newShareds->blockCountPerMip[mip] )
		{
			const u32 refBlockOrder = refShareds->blockOffsetPerMip[mip] + refBlockRank;
			const u32 newBlockOrder = newShareds->blockOffsetPerMip[mip] + newBlockRank;
			const u32 refBlockID = refFrame->blockIDs[refBlockOrder];
			const u32 newBlockID = newFrame->blockIDs[newBlockOrder];
			Block_s* refBlock = &refFrame->blocks[refBlockID];
			Block_s* newBlock = &newFrame->blocks[newBlockID];
			if ( refBlock->key == newBlock->key )
			{
				if ( Block_HasSimilarData( refBlock, newBlock ) )
				{
					Block_LinkData( refBlock, newBlock );

					BitSet_SetBit( &refSharedBlockBitSet, refBlockOrder - refShareds->blockOffsetPerMip[0] );
					BitSet_SetBit( &newSharedBlockBitSet, newBlockOrder - newShareds->blockOffsetPerMip[0] );

					++sharedCount;
				}

				++refBlockRank;
				++newBlockRank;
			}
			else if ( refBlock->key < newBlock->key )
				++refBlockRank;
			else
				++newBlockRank;
		}
	}

	if ( sharedCount )
	{
		RawFrame_TrimShared( refFrameID, sharedFrameID, &refSharedBlockBitSet, context );
		RawFrame_TrimShared( newFrameID, 0, &newSharedBlockBitSet, context );
		memset( &newFrame->shareds[1], 0, sizeof( RawFrame_s::Shared_s ) );
	}

	context->stack->pop();

	return sharedCount;
}

static void RawFrame_InitRangeAndBucketFrames( u32 frameID, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameID];

	BucketFrame_s* bucketFrames[CODEC_BUCKET_COUNT];
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		bucketFrames[bucket] = &context->bucketFrames[bucket][frameID];
		memset( bucketFrames[bucket], 0, sizeof( BucketFrame_s ) );
	}

	{
		context->stack->push();

		u32* perBucketBlockIDs[CODEC_BUCKET_COUNT] = {};

		u32 blockOffset = 0;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			BucketFrame_s* bucketFrame = bucketFrames[bucket];
			u32* blockIDs = context->stack->newArray< u32 >( frame->blockCount );

			for ( u32 sharedFrameID = 0; sharedFrameID < ENCODER_SHARED_FRAME_MAX_COUNT; ++sharedFrameID )
			{
				const RawFrame_s::Shared_s* shared = &frame->shareds[sharedFrameID];
				if ( shared->blockCount == 0 )
					continue;

				for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
				{
					u32* mipBlockCount = &bucketFrame->shareds[sharedFrameID].blockCountPerMip[mip];
					V6_ASSERT( *mipBlockCount == 0 );
					for ( u32 blockRank = 0; blockRank < shared->blockCountPerMip[mip]; ++blockRank )
					{
						const u32 blockOrder = shared->blockOffsetPerMip[mip] + blockRank;
						const u32 blockID = frame->blockIDs[blockOrder];
						const Block_s* block = &frame->blocks[blockID];
						if ( block->bucket == bucket )
						{
							blockIDs[bucketFrame->blockCount + *mipBlockCount] = blockID;
							++(*mipBlockCount);
						}
					}
					bucketFrame->blockCount += *mipBlockCount;
				}
			}

			for ( u32 sharedFrameID = 0; sharedFrameID < ENCODER_SHARED_FRAME_MAX_COUNT; ++sharedFrameID )
			{
				BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameID];

				for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
				{
					if ( shared->blockCountPerMip[mip] )
					{
						V6_ASSERT( context->rangeCounts[bucket] < ENCODER_RANGE_MAX_COUNT );

						const u32 rangeID = context->rangeCounts[bucket];
						CodecRange_s* range = &context->ranges[bucket][rangeID];
						range->gridOrg = frame->gridMin[mip] + (int)context->gridMacroHalfWidth;
						range->blockOffset = context->blockGlobalCount + blockOffset;

						shared->rangeIDs[mip] = rangeID;
						++context->rangeCounts[bucket];
					}
					else
					{
						shared->rangeIDs[mip] = ENCODER_EMPTY_RANGE;
					}
					shared->blockCount += shared->blockCountPerMip[mip];
					shared->blockOffsetPerMip[mip] = blockOffset;
					blockOffset += shared->blockCountPerMip[mip];
				}
			}

			perBucketBlockIDs[bucket] = blockIDs;
		}

		u32 blockCount = 0;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			BucketFrame_s* bucketFrame = bucketFrames[bucket];
			memcpy( frame->blockIDs + bucketFrame->shareds[0].blockOffsetPerMip[0], perBucketBlockIDs[bucket], bucketFrame->blockCount * sizeof( u32 ) );
			blockCount += bucketFrame->blockCount;
		}

		context->blockGlobalCount += blockCount;

		context->stack->pop();
	}
}

static u32 BucketFrame_WriteBlocks( u32 bucket, u32 frameID, IStreamWriter* blockPosWriter, IStreamWriter* blockDataWriter, Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameID];
	BucketFrame_s* bucketFrame = &context->bucketFrames[bucket][frameID];

	if ( bucketFrame->blockCount == 0 )
		return 0;

	const u32 cellCount = 1 << (bucket + 2);

	for ( u32 sharedFrameID = 0; sharedFrameID < ENCODER_SHARED_FRAME_MAX_COUNT; ++sharedFrameID )
	{
		BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameID];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
		{
			for ( u32 blockRank = 0; blockRank < shared->blockCountPerMip[mip]; ++blockRank )
			{
				const u32 blockOrder = shared->blockOffsetPerMip[mip] + blockRank;
				const u32 blockID = frame->blockIDs[blockOrder];
				const Block_s* block = &frame->blocks[blockID];
				const u32 packedBlockPos = (block->mip << 28) | block->pos;
				blockPosWriter->Write( &packedBlockPos, sizeof( u32 ) );
				blockDataWriter->Write( block->data, cellCount * sizeof( u32 ) );
			}
		}
	}

	return bucketFrame->blockCount;
}

static u32 BucketFrame_WriteGroups( u32 bucket, u32 refFrameID, u32 frameID, IStreamWriter* streamWriter, Context_s* context )
{
	V6_ASSERT( refFrameID <= frameID );
	const RawFrame_s* frame = &context->frames[frameID];

	BucketFrame_s* bucketFrame = &context->bucketFrames[bucket][refFrameID];

	if ( bucketFrame->blockCount == 0 )
		return 0;

	u32 bucketGroupCount = 0;

	const u32 minSharedFrameID = frameID - refFrameID;
	for ( u32 sharedFrameID = minSharedFrameID; sharedFrameID < ENCODER_SHARED_FRAME_MAX_COUNT; ++sharedFrameID )
	{
		BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameID];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
		{
			if ( !shared->blockCountPerMip[mip] )
				continue;

			const u32 rangeID = shared->rangeIDs[mip];

			V6_ASSERT( rangeID != ENCODER_EMPTY_RANGE );
			V6_ASSERT( (rangeID & 0xFFFF) == rangeID );

			context->stack->push();

			u32 groupCount = ((shared->blockCountPerMip[mip] + 127) / 128) * 2;
			u16* chunk = context->stack->newArray< u16 >( groupCount );

			for ( u32 group = 0; group < groupCount; ++group )
				chunk[group] = group < shared->blockCountPerMip[mip] ? (u16)rangeID : ENCODER_RANGE_MAX_COUNT;

			streamWriter->Write( chunk, groupCount * sizeof( u16 ) );

			context->stack->pop();

			bucketGroupCount += groupCount;
		}
	}

	return bucketGroupCount * 64;
}

static void Context_WriteStreamHeader( IStreamWriter* streamWriter, Context_s* context )
{
	CodecStreamDesc_s desc = {};
	desc.frameCount = context->frameCount;
	desc.sampleCount = context->sampleCount;
	desc.gridMacroShift = context->gridMacroShift;
	desc.gridScaleMin = context->gridScaleMin;
	desc.gridScaleMax = context->gridScaleMax;

	Codec_WriteStreamHeader( streamWriter, &desc );
}

static void RawFrame_WriteAndRelease( u32 frameID, IStreamWriter* streamWriter, Context_s* context )
{
	context->stack->push();

	CMemoryWriter memoryRangeWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(  1 ) );
	CMemoryWriter memoryBlockPosWriter(		context->stack->alloc( MulMB(  2 ) ),	MulMB(  2 ) );
	CMemoryWriter memoryBlockDataWriter(	context->stack->alloc( MulMB( 30 ) ),	MulMB( 30 ) );
	CMemoryWriter memoryGroupWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(  1 ) );

	u32 dataBlockCounts[CODEC_BUCKET_COUNT] = {};
	u32 usedBlockCounts[CODEC_BUCKET_COUNT] = {};

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		dataBlockCounts[bucket] = BucketFrame_WriteBlocks( bucket, frameID, &memoryBlockPosWriter, &memoryBlockDataWriter, context );

		for ( u32 refFrameID = 0; refFrameID <= frameID; ++refFrameID )
			usedBlockCounts[bucket] += BucketFrame_WriteGroups( bucket, refFrameID, frameID, &memoryGroupWriter, context );
	}

	if ( frameID == 0 )
	{
		CodecIFrameDesc_s desc = {};
		desc.frame = frameID;
		memcpy( desc.rangeCounts, context->rangeCounts, sizeof( desc.rangeCounts ) );
		memcpy( desc.dataBlockCounts, dataBlockCounts, sizeof( desc.dataBlockCounts ) );
		memcpy( desc.usedBlockCounts, usedBlockCounts, sizeof( desc.usedBlockCounts ) );

		CodecIFrameData_s data = {};
		data.ranges = (CodecRange_s*)memoryRangeWriter.GetBuffer();
		data.blockPos = (u32*)memoryBlockPosWriter.GetBuffer();
		data.blockData = (u32*)memoryBlockDataWriter.GetBuffer();
		data.groups = (u16*)memoryRangeWriter.GetBuffer();

		Codec_WriteIFrame( streamWriter, &desc, &data );
	}
	else
	{
		CodecPFrameDesc_s desc = {};
		desc.frame = frameID;
		memcpy( desc.dataBlockCounts, dataBlockCounts, sizeof( desc.dataBlockCounts ) );
		memcpy( desc.usedBlockCounts, usedBlockCounts, sizeof( desc.usedBlockCounts ) );

		CodecPFrameData_s data = {};
		data.blockPos = (u32*)memoryBlockPosWriter.GetBuffer();
		data.blockData = (u32*)memoryBlockDataWriter.GetBuffer();
		data.groups = (u16*)memoryRangeWriter.GetBuffer();

		Codec_WritePFrame( streamWriter, &desc, &data );
	}

	V6_MSG( "F%02d: ranges %d KB, blockPos %d KB, blockData %d KB, groups %d KB.\n", 
		frameID,
		DivKB( memoryRangeWriter.GetSize() ),
		DivKB( memoryBlockPosWriter.GetSize() ),
		DivKB( memoryBlockDataWriter.GetSize() ),
		DivKB( memoryGroupWriter.GetSize() ) );

	context->stack->pop();

	const RawFrame_s* frame = &context->frames[frameID];
	context->heap->free( frame->blocks );
	context->heap->free( frame->blockIDs );
}

bool Encoder_EncodeFrames( const char* templateFilename, u32 fileCount, const char* streamFilename, IAllocator* heap )
{
	if ( fileCount == 0 || fileCount > ENCODER_SHARED_FRAME_MAX_COUNT )
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

	Stack stack( heap, 100 * 1024 * 1024 );

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
			return 1;

		V6_MSG( "F%02d: loaded %d blocks from %s.\n", frameID, context->frames[frameID].blockCount, filename );
	}

	V6_MSG( "Sorting by key...\n" );
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{
		RawFrame_SortByKey( frameID, context );
		V6_MSG( "F%02d: sorted.\n", frameID );
	}

	V6_MSG( "Merging...\n" );
	for ( u32 newFrameID = 1; newFrameID < context->frameCount; ++newFrameID )
	{
		u32 uniqueCount = context->frames[newFrameID].blockCount;
		V6_MSG( "F%02d: %d blocks split into", newFrameID, uniqueCount );
		for ( u32 refFrameID = 0; refFrameID < newFrameID; ++refFrameID )
		{
			const u32 sharedCount = RawFrame_Merge( refFrameID, newFrameID, context );
			uniqueCount -= sharedCount;
			if ( sharedCount )
				V6_PRINT( " %02d%%", sharedCount * 100 / context->frames[newFrameID].blockCount );
		}
		V6_PRINT( " %02d%%.\n", uniqueCount * 100 / context->frames[newFrameID].blockCount );
	}

	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
		RawFrame_InitRangeAndBucketFrames( frameID, context );

	V6_MSG( "Writing...\n" );

	Context_WriteStreamHeader( &fileWriter, context );

	u32 prevFileSize = fileWriter.GetPos();
	for ( u32 frameID = 0; frameID < context->frameCount; ++frameID )
	{		
		RawFrame_WriteAndRelease( frameID, &fileWriter, context );
		V6_MSG( "F%02d: added %d KB.\n", frameID, DivKB( fileWriter.GetPos() - prevFileSize ) );
		prevFileSize = fileWriter.GetPos();
	}

	V6_PRINT( "\n" );
	V6_MSG( "Stream: %d KB, avg of %d KB/frame\n", DivKB( fileWriter.GetSize() ), DivKB( fileWriter.GetSize() / context->frameCount ) );

	return true;
}

END_V6_CORE_NAMESPACE
