/*V6*/

#pragma comment( lib, "core.lib" )

#include <v6/core/common.h>

#include <v6/core/bit.h>
#include <v6/core/codec.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/vec3i.h>

#define ENCODER_SHARED_FRAME_MAX_COUNT	256
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

struct Frame_s
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

struct Context_s
{
	IAllocator*	heap;
	IStack*		stack;
	Frame_s*	frames;
	u32			frameCount;
	u32			gridMacroShift;
	u32			gridMacroWidth;
	u32			gridMacroHalfWidth;
	float		gridScaleMin;
	float		gridScaleMax;
	u32			mipCount;
	Vec3i		gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i		gridMax[CODEC_MIP_MAX_COUNT];
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
	Frame_s* frame = (Frame_s*)framePointer;
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

		if ( Abs( (int)(refR - newR) ) > ENCODER_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refG - newG) ) > ENCODER_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(refB - newB) ) > ENCODER_COLOR_ERROR_TOLERANCE )
			return false;
		
		if ( refBlock->merged )
			return Block_HasSimilarData( refBlock->merged, newBlock );
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

bool Frame_LoadFromFile( u32 frameID, const char* filename, Context_s* context )
{
	Frame_s* frame = &context->frames[frameID];
	memset( frame, 0, sizeof( Frame_s ) );

	CFileReader fileReader;
	if ( !fileReader.Open( filename ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return false;
	}

	context->stack->push();

	CodecFrameDesc_s desc;
	CodecFrameData_s data;

	if  ( !Codec_ReadFrame( &fileReader, &desc, &data, context->stack ) )
	{
		context->stack->pop();

		V6_ERROR( "Unable to read %s.\n", filename );
		return false;
	}

	if ( frameID == 0 )
	{
		context->gridMacroShift = desc.gridMacroShift;
		context->gridMacroWidth = 1 << context->gridMacroShift;
		context->gridScaleMin = desc.gridScaleMin;
		context->gridScaleMax = desc.gridScaleMax;
		context->gridMacroHalfWidth = context->gridMacroWidth >> 1;
		context->mipCount = Codec_GetMipCount( &desc );
	}
	else
	{
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

void Frame_Sort( u32 frameID, Context_s* context )
{
	Frame_s* frame = &context->frames[frameID];

	frame->blockIDs = context->heap->newArray< u32 >( frame->blockCount );
	for ( u32 blockID = 0; blockID < frame->blockCount; ++blockID )
	{
		Block_s* block = &frame->blocks[blockID];
		block->key = ComputeKeyFromMipAndBlockPos( block->mip, block->pos, context->gridMacroShift, -context->gridMin[block->mip] );
		frame->blockIDs[blockID] = blockID;
	}

	qsort_s( frame->blockIDs, frame->blockCount, sizeof( core::u32 ), Block_CompareKey, frame );

	for ( u32 mip = 0; mip < context->mipCount; ++mip )
		V6_ASSERT( frame->shareds[0].blockCountPerMip[mip] == 0 || (
			frame->blocks[frame->blockIDs[frame->shareds[0].blockOffsetPerMip[mip]]].mip == mip && 
			frame->blocks[frame->blockIDs[frame->shareds[0].blockOffsetPerMip[mip] + frame->shareds[0].blockCountPerMip[mip] - 1]].mip == mip) );
}

void Frame_TrimShared( u32 frameID, u32 sharedFrameID, BitSet_s* sharedBlockBitSet, Context_s* context )
{
	V6_ASSERT( sharedFrameID+1 < ENCODER_SHARED_FRAME_MAX_COUNT );

	Frame_s* frame = &context->frames[frameID];

	Frame_s::Shared_s* unique = &frame->shareds[sharedFrameID];
	Frame_s::Shared_s* shared = &frame->shareds[sharedFrameID+1];
	memset( shared, 0, sizeof( *shared ) );

	context->stack->push();

	u32* uniqueBlockIDs = context->stack->newArray< u32 >( BitSet_GetSize( unique->blockCount ) );
	u32* sharedBlockIDs = context->stack->newArray< u32 >( BitSet_GetSize( unique->blockCount ) );

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

	V6_MSG( "Frame %d: split %d blocks in %d unique blocks and %d shared blocks.\n", frameID, unique->blockCount + shared->blockCount, unique->blockCount, shared->blockCount );
}

void Frame_Merge( u32 refFrameID, u32 newFrameID, Context_s* context )
{
	V6_ASSERT( refFrameID < newFrameID );
	Frame_s* refFrame = &context->frames[refFrameID];
	Frame_s* newFrame = &context->frames[newFrameID];
	
	u32 sharedCount = 0;

	Frame_s::Shared_s* refShareds = &refFrame->shareds[newFrameID-1];
	Frame_s::Shared_s* newShareds = &newFrame->shareds[0];

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
		Frame_TrimShared( refFrameID, newFrameID-1, &refSharedBlockBitSet, context );
		Frame_TrimShared( newFrameID, 0, &newSharedBlockBitSet, context );
	}

	context->stack->pop();

	V6_MSG( "%d shared blocks.\n", sharedCount );
}

int Encoder_EncodeFrames( const char* filenameTemplate, u32 fileCount, IAllocator* heap )
{
	if ( fileCount < 1 )
		return 0;

	Stack stack( heap, 100 * 1024 * 1024 );

	Context_s context = {};

	context.heap = heap;
	context.stack = &stack;
	context.frames = stack.newArray< Frame_s >( fileCount );
	context.frameCount = fileCount;

	// Load all frames
	
	V6_MSG( "Loading...\n" );
	for ( u32 frameID = 0; frameID < context.frameCount; ++frameID )
	{
		char filename[256];
		sprintf_s( filename, sizeof( filename ), filenameTemplate, frameID );

		if ( !Frame_LoadFromFile( frameID, filename, &context ) )
			return 1;

		V6_MSG( "Frame %d: loaded %d blocks from %s.\n", frameID, context.frames[frameID].blockCount, filename );
	}

	V6_MSG( "Sorting...\n" );
	for ( u32 frameID = 0; frameID < context.frameCount; ++frameID )
		Frame_Sort( frameID, &context );
	
	V6_MSG( "Merging...\n" );
	for ( u32 newFrameID = 1; newFrameID < context.frameCount; ++newFrameID )
	{
		for ( u32 refFrameID = 0; refFrameID < newFrameID; ++refFrameID )
			Frame_Merge( refFrameID, newFrameID, &context );
	}

	return 0;
}

END_V6_CORE_NAMESPACE

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::core::CHeap heap;

	const char* filenameTemplate = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";

	return v6::core::Encoder_EncodeFrames( filenameTemplate, 3, &heap );
}