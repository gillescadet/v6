/*V6*/

#include <v6/core/common.h>

#include <v6/codec/encoder.h>
#include <v6/codec/codec.h>
#include <v6/core/bit.h>
#include <v6/core/color.h>
#include <v6/core/image.h>
#include <v6/core/memory.h>
#include <v6/core/plot.h>
#include <v6/core/stream.h>
#include <v6/core/string.h>
#include <v6/core/vec3i.h>

#define ENCODER_DEBUG					0

#define ENCODER_EMPTY_RANGE				0xFFFFFFFF
#define ENCODER_EMPTY_CELL				0xFFFFFFFF

#define ENCODER_BC1_WIP					0

BEGIN_V6_NAMESPACE

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
	u32*		blockIDs;
	u32			blockCount;
	u32			refFrameRank;
	Vec3		gridOrigin;
	Vec3		gridBasis[3];
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

struct ContextStream_s
{
	IAllocator*			heap;
	Stack*				stack;
	CodecStreamDesc_s	desc;
	CodecStreamData_s	data;
	u32					gridMacroWidth;
	u32					gridMacroHalfWidth;
	u32					mipCount;
};

struct Context_s
{
	ContextStream_s*	stream;
	IAllocator*			heap;
	IStack*				stack;
	RawFrame_s*			frames;
	BucketFrame_s*		bucketFrames[CODEC_BUCKET_COUNT];
	Vec3i				gridMin[CODEC_MIP_MAX_COUNT];
	Vec3i				gridMax[CODEC_MIP_MAX_COUNT];
	CodecRange_s		rangeDefs[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	u32					rangeDefCounts[CODEC_BUCKET_COUNT];
	u32					frameCount;
	u32					blockPosCountPerSequence;
	u32					blockDataCountPerSequence;
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

static u32 Block_GetBucket( u32 cellCount )
{
	V6_ASSERT( cellCount );
	u32 bit = Bit_GetFirstBitHigh( (u32)cellCount-1 );
	return Max( (bit == (u32)-1) ? 0u : bit, 1u ) - 1;
}

#if 0

static int Block_CompareEncoded( void* bufferPointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	const EncodedBlock_s* encodedBlocks = (EncodedBlock_s*)bufferPointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	return memcmp( &encodedBlocks[blockID0], &encodedBlocks[blockID1], sizeof( EncodedBlock_s ) );
}

static int Block_CompareEncodedEx( void* bufferPointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	const EncodedBlockEx_s* encodedBlockExs = (EncodedBlockEx_s*)bufferPointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	return memcmp( &encodedBlockExs[blockID0], &encodedBlockExs[blockID1], sizeof( EncodedBlockEx_s ) );
}

#endif

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
	frame->gridBasis[0] = desc.gridBasis[0];
	frame->gridBasis[1] = desc.gridBasis[1];
	frame->gridBasis[2] = desc.gridBasis[2];
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

	u32 blockPosOffsets[CODEC_BUCKET_COUNT];
	u32 blockDataOffsets[CODEC_BUCKET_COUNT];

	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

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
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank )
		{
#if ENCODER_DEBUG == 1
			Plot_NewObject( &plot, Color_Make( 255, 0, 0, 50 ) );
#endif // #if ENCODER_DEBUG == 1

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
#if ENCODER_DEBUG == 1
					const Vec3u cellCoords = ComputeCellCoords( block->pos, context->stream->desc.gridMacroShift, cellPos );
					Vec3 pMin;
					pMin.x = gridCenters[block->mip].x + (cellCoords.x * halfCellSizes[block->mip] * 2.0f ) - gridScales[block->mip];
					pMin.y = gridCenters[block->mip].y + (cellCoords.y * halfCellSizes[block->mip] * 2.0f ) - gridScales[block->mip];
					pMin.z = gridCenters[block->mip].z + (cellCoords.z * halfCellSizes[block->mip] * 2.0f ) - gridScales[block->mip];
					const Vec3 pMax = pMin + halfCellSizes[block->mip] * 2.0f;
					Plot_AddBox( &plot, &pMin, &pMax, false );
					Plot_AddBox( &plot, &pMin, &pMax, true );
#endif // #if ENCODER_DEBUG == 1
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
}

static void RawFrame_GenerateBitmaps( u32 frameRank, Context_s* context )
{
	ScopedStack scopedStack( context->stack );
	
	RawFrame_s* frame = &context->frames[frameRank];

	for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
	{
		const u32 blockCount = frame->blockCountPerMip[mip];
		u32 width = 1;
		while ( width * width < blockCount )
			width *= 2;

		Image_s image;
		Image_Create( &image, context->stack, width * 8, width * 8);
		memset( image.pixels, 0, Image_GetSize( &image ) );
		
		for ( u32 blockOrder = 0; blockOrder < blockCount; ++blockOrder )
		{
			const u32 blockID = frame->blockIDs[blockOrder];
			const Block_s* block = &frame->blocks[blockID];

			int x, y;
			d2xy( width, blockOrder, &x, &y );
			x *= 8;
			y *= 8;

			u32 sumR = 0;
			u32 sumG = 0;
			u32 sumB = 0;
			u32 cellCount = 0;

			u64 cellPresence = block->cellPresence;
			V6_ASSERT( cellPresence );
			do
			{
				const u32 cellPos = Bit_GetFirstBitHigh( cellPresence );
				cellPresence -= 1ll << cellPos;
				
				int i, j;
				d2xy( 8, cellPos, &i, &j );

				const u32 pixelID = (y + j) * (width * 8) + x + i;
				
				Color_s* color = &image.pixels[pixelID];
				color->r = (block->cellRGBA[cellPos] >> 24) & 0xFF;
				color->g = (block->cellRGBA[cellPos] >> 16) & 0xFF;
				color->b = (block->cellRGBA[cellPos] >>  8) & 0xFF;
				color->a = 255;

				sumR += color->r;
				sumG += color->g;
				sumB += color->b;
				++cellCount;
			} while ( cellPresence != 0 );

			const u32 r = sumR / cellCount;
			const u32 g = sumG / cellCount;
			const u32 b = sumB / cellCount;

			for ( u32 j = 0; j < 8; ++j )
			{
				for ( u32 i = 0; i < 8; ++i )
				{
					const u32 pixelID = (y + j) * (width * 8) + x + i;
					Color_s* color = &image.pixels[pixelID];
					if ( color->a == 0 )
					{
						color->r = r;
						color->g = g;
						color->b = b;
						color->a = 255;
					}
				}
			}
		}

		char filename[256];
		sprintf_s( filename, sizeof( filename ), "d:/tmp/frame_%06d_mip_%02d.bmp", frameRank, mip );
		CFileWriter fileWriter;
		if ( fileWriter.Open( filename ) )
			Image_WriteBitmap( &image, &fileWriter );
	}
}

static void RawFrame_SortByKey( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

	frame->blockIDs = context->heap->newArray< u32 >( frame->blockCount );
	for ( u32 blockID = 0; blockID < frame->blockCount; ++blockID )
	{
		Block_s* block = &frame->blocks[blockID];
		block->key = ComputeKeyFromMipAndBlockPos( block->mip, block->pos, context->stream->desc.gridMacroShift, frame->gridMin[block->mip] - context->gridMin[block->mip] );
		V6_ASSERT( block->key != 0 );
		frame->blockIDs[blockID] = blockID;
	}

	qsort_s( frame->blockIDs, frame->blockCount, sizeof( u32 ), Block_CompareKey, frame );

	for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
		V6_ASSERT( frame->blockCountPerMip[mip] == 0 || (
			frame->blocks[frame->blockIDs[frame->blockOffsetPerMip[mip]]].mip == mip && 
			frame->blocks[frame->blockIDs[frame->blockOffsetPerMip[mip] + frame->blockCountPerMip[mip] - 1]].mip == mip) );
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

		if ( !block->nextFrame )
			continue;

		block->nextFrame->linked = false;
	}

	frame->refFrameRank = refFrameRank;
}

static u32 RawFrame_Merge( u32 frameRank, Context_s* context )
{
	RawFrame_s* frame = &context->frames[frameRank];

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

	qsort_s( frame->blockIDs, rootCount, sizeof( u32 ), Block_CompareByBucketThenBySharedFrameCountThenByKey2, frame );

	BucketFrame_s* bucketFrames[CODEC_BUCKET_COUNT];
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		bucketFrames[bucket] = &context->bucketFrames[bucket][frameRank];
		memset( bucketFrames[bucket], 0, sizeof( BucketFrame_s ) );
	}

	for ( u32 blockOrder = 0; blockOrder < rootCount; ++blockOrder )
	{
		const u32 blockID = frame->blockIDs[blockOrder];
		const Block_s* block = &frame->blocks[blockID];
		const u32 bucket = block->bucket;
		const u32 sharedFrameRank = block->sharedFrameCount;
		++bucketFrames[bucket]->shareds[sharedFrameRank].blockCountPerMip[block->mip];
	}

	u32 blockOffset = 0;
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		BucketFrame_s* bucketFrame = bucketFrames[bucket];
		for ( u32 sharedFrameRank = 0; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
		{
			BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameRank];

			for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
			{
				if ( shared->blockCountPerMip[mip] )
				{
					V6_ASSERT( context->rangeDefCounts[bucket] < CODEC_RANGE_MAX_COUNT );

					const u32 rangeID = context->rangeDefCounts[bucket];
					CodecRange_s* range = &context->rangeDefs[bucket][rangeID];
					V6_ASSERT( frameRank <= 0xFF);
					V6_ASSERT( mip <= 0xF );
					V6_ASSERT( shared->blockCountPerMip[mip] <= 0xFFFFF );
					range->frameRank8_mip4_blockCount20 = (frameRank << 24) | (mip << 20) | shared->blockCountPerMip[mip];

					shared->rangeIDs[mip] = rangeID;
					++context->rangeDefCounts[bucket];

					//V6_MSG( "F%02d: bucket %d, shared %d, mip %d, blocks %8d.\n", frameRank, bucket, sharedFrameRank, mip, shared->blockCountPerMip[mip] );
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

static void RawFrame_UpdateLimits( u32 frameRank, const CodecFrameDesc_s* desc, const CodecFrameData_s* data, Context_s* context )
{
	V6_ASSERT( RawFrame_IsRefFrame( frameRank, context ) );

	u32 frameUniqueBlockPosCount = 0;
	u32 frameUniqueBlockDataCount = 0;
	u32 frameBlockRangeCount = 0;
	u32 frameBlockCount = 0;
	u32 frameBlockGroupCount = 0;
		
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 rangeID = 0; rangeID < context->rangeDefCounts[bucket]; ++rangeID )
		{
			const CodecRange_s* codecRange = &context->rangeDefs[bucket][rangeID];
			
			const u32 rangeFrameRank = codecRange->frameRank8_mip4_blockCount20 >> 24;
			if ( rangeFrameRank != frameRank )
				continue;
			
			const u32 blockCount = codecRange->frameRank8_mip4_blockCount20 & 0xFFFFF;
			frameUniqueBlockPosCount += blockCount;
			frameUniqueBlockDataCount += blockCount * cellPerBucketCount;
		}

		const u32 bucketBlockRangeCount = desc->blockRangeCounts[bucket];

		for ( u32 rangeRank = 0; rangeRank < bucketBlockRangeCount; ++rangeRank )
		{
			const u32 rangeID = data->rangeIDs[rangeRank];
			const u32 blockCount = context->rangeDefs[bucket][rangeID].frameRank8_mip4_blockCount20 & 0xFFFFF;
			frameBlockCount += blockCount;
			frameBlockGroupCount += (blockCount + CODEC_BLOCK_THREAD_GROUP_SIZE - 1) / CODEC_BLOCK_THREAD_GROUP_SIZE;
		}

		frameBlockRangeCount += bucketBlockRangeCount;
	}

	context->stream->desc.maxBlockRangeCountPerFrame = Max( context->stream->desc.maxBlockRangeCountPerFrame, frameBlockRangeCount );
	context->stream->desc.maxBlockCountPerFrame = Max( context->stream->desc.maxBlockCountPerFrame, frameBlockCount );
	context->stream->desc.maxBlockGroupCountPerFrame = Max( context->stream->desc.maxBlockGroupCountPerFrame, frameBlockGroupCount );

	context->blockPosCountPerSequence += frameUniqueBlockPosCount;
	context->blockDataCountPerSequence += frameUniqueBlockDataCount;
}

static u32 BucketFrame_WriteBlocks( u32 bucket, u32 frameRank, IStreamWriter* blockPosWriter, IStreamWriter* blockDataWriter, Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameRank];
	BucketFrame_s* bucketFrame = &context->bucketFrames[bucket][frameRank];

	const u32 emptyRGBA = ENCODER_EMPTY_CELL;

	if ( bucketFrame->blockCount == 0 )
		return 0;

	const u32 perBucketCellCount = 1 << (bucket + 2);

	for ( u32 sharedFrameRank = 0; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
	{
		BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameRank];
		if ( shared->blockCount == 0 )
			continue;

		for ( u32 mip = 0; mip < context->stream->mipCount; ++mip )
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
				const u32 cellPerBucketCount = 1 << (2 + bucket);
				while ( cellCount < cellPerBucketCount )
				{
					cellRGBA[cellCount] = ENCODER_EMPTY_CELL;
					++cellCount;
				}
				blockDataWriter->Write( cellRGBA, cellCount * sizeof( u32 ) );
			}
		}
	}

	return bucketFrame->blockCount;
}

static u32 BucketFrame_WriteRangeIDs( u32 bucket, u32 refFrameRank, u32 frameRank, IStreamWriter* streamWriter, Context_s* context )
{
	V6_ASSERT( refFrameRank <= frameRank );
	const RawFrame_s* frame = &context->frames[frameRank];

	BucketFrame_s* bucketFrame = &context->bucketFrames[bucket][refFrameRank];

	if ( bucketFrame->blockCount == 0 )
		return 0;

	u32 rangeCount = 0;

	const u32 minSharedFrameRank = frameRank - refFrameRank;
	for ( u32 sharedFrameRank = minSharedFrameRank; sharedFrameRank < CODEC_FRAME_MAX_COUNT; ++sharedFrameRank )
	{
		BucketFrame_s::Shared_s* shared = &bucketFrame->shareds[sharedFrameRank];
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

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		memoryRangeDefWriter.Write( context->rangeDefs[bucket], context->rangeDefCounts[bucket] * sizeof( CodecRange_s ) );

	{
		CodecSequenceDesc_s desc = {};
		desc.sequenceID = sequenceID;
		desc.frameCount = context->frameCount;
		memcpy( desc.rangeDefCounts, context->rangeDefCounts, sizeof( context->rangeDefCounts ) );

		CodecSequenceData_s data = {};
		data.rangeDefs = (CodecRange_s*)memoryRangeDefWriter.GetBuffer();

		Codec_WriteSequence( streamWriter, &desc, &data );
	}

	V6_MSG( "Header: range defs %d KB.\n", DivKB( memoryRangeDefWriter.GetSize() ) );
}

static bool RawFrame_Write( u32 frameRank, IStreamWriter* streamWriter, Context_s* context )
{
	const RawFrame_s* frame = &context->frames[frameRank];

	ScopedStack scopedStack( context->stack );

	CBufferWriter memoryBlockPosWriter(		context->stack->alloc( MulMB(  10 ) ),	MulMB(  10 ) );
	CBufferWriter memoryBlockDataWriter(	context->stack->alloc( MulMB( 200 ) ),	MulMB( 200 ) );
	CBufferWriter memoryRangeIDWriter(		context->stack->alloc( MulMB(  1 ) ),	MulMB(   1 ) );

	u32 blockCounts[CODEC_BUCKET_COUNT] = {};
	u32 rangeCounts[CODEC_BUCKET_COUNT] = {};

	const bool refFrame = RawFrame_IsRefFrame( frameRank, context );

	if ( refFrame )
	{
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			blockCounts[bucket] = BucketFrame_WriteBlocks( bucket, frameRank, &memoryBlockPosWriter, &memoryBlockDataWriter, context );

			for ( u32 refFrameRank = 0; refFrameRank <= frameRank; ++refFrameRank )
				rangeCounts[bucket] += BucketFrame_WriteRangeIDs( bucket, refFrameRank, frameRank, &memoryRangeIDWriter, context );
		}
	}

#if 0
	if ( refFrame )
	{
		ScopedStack scopedStack2( context->stack );

		const u32 lessThan32Count = blockCounts[0] + blockCounts[1] + blockCounts[2] + blockCounts[3];
		const u32 moreThan32Count = blockCounts[4];
		const u32 blockCount = lessThan32Count + moreThan32Count;

		u32* blockIDs = context->stack->newArray< u32 >( lessThan32Count );
		for ( u32 blockID = 0; blockID < lessThan32Count; ++blockID )
			blockIDs[blockID] = blockID;
		
		u32* blockIDExs = context->stack->newArray< u32 >( moreThan32Count );
		for ( u32 blockID = 0; blockID < moreThan32Count; ++blockID )
			blockIDExs[blockID] = blockID;

		qsort_s( blockIDs, lessThan32Count, sizeof( u32 ), Block_CompareEncoded, memoryBlockDataWriter.GetBuffer() );
		qsort_s( blockIDExs, moreThan32Count, sizeof( u32 ), Block_CompareEncodedEx, (EncodedBlock_s*)memoryBlockDataWriter.GetBuffer() + lessThan32Count );
		
		EncodedBlock_s* encodedBlocks = (EncodedBlock_s*)memoryBlockDataWriter.GetBuffer();
		u32 sameBlockCount = 0;
		for ( u32 blockOrder = 1; blockOrder < lessThan32Count; ++blockOrder )
		{
			const u32 blockID0 = blockIDs[blockOrder-1];
			const u32 blockID1 = blockIDs[blockOrder];
			sameBlockCount += memcmp( &encodedBlocks[blockID0], &encodedBlocks[blockID1], sizeof( EncodedBlock_s) ) == 0;
		}
		
		EncodedBlockEx_s* encodedBlockExs = (EncodedBlockEx_s*)((EncodedBlock_s*)memoryBlockDataWriter.GetBuffer() + lessThan32Count);
		u32 sameBlockExCount = 0;
		for ( u32 blockOrder = 1; blockOrder < moreThan32Count; ++blockOrder )
		{
			const u32 blockID0 = blockIDExs[blockOrder-1];
			const u32 blockID1 = blockIDExs[blockOrder];
			sameBlockExCount += memcmp( &encodedBlockExs[blockID0], &encodedBlockExs[blockID1], sizeof( EncodedBlockEx_s) ) == 0;
		}

		V6_MSG( "F%02d: %d unique encoded block (%d/%d + %d/%d).\n", frameRank, sameBlockCount + sameBlockExCount, sameBlockCount, lessThan32Count, sameBlockExCount, moreThan32Count );
	}
#endif

	CodecFrameDesc_s frameDesc = {};
	frameDesc.gridOrigin = frame->gridOrigin;
	frameDesc.gridBasis[0] = frame->gridBasis[0];
	frameDesc.gridBasis[1] = frame->gridBasis[1];
	frameDesc.gridBasis[2] = frame->gridBasis[2];
	frameDesc.frameRank = frameRank;

	if ( refFrame )
	{
		memcpy( frameDesc.blockCounts, blockCounts, sizeof( frameDesc.blockCounts ) );
		memcpy( frameDesc.blockRangeCounts, rangeCounts, sizeof( frameDesc.blockRangeCounts ) );

		CodecFrameData_s frameData = {};
		frameData.blockPos = (u32*)memoryBlockPosWriter.GetBuffer();
		frameData.blockData = (u32*)memoryBlockDataWriter.GetBuffer();
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
	context->stream->desc.maxBlockPosCountPerSequence = Max( context->stream->desc.maxBlockPosCountPerSequence, context->blockPosCountPerSequence );
	context->stream->desc.maxBlockDataCountPerSequence = Max( context->stream->desc.maxBlockDataCountPerSequence, context->blockDataCountPerSequence );
}

static bool ContextStream_EncodeSequence( IStreamWriter* streamWriter, const char* templateRawFilename, u32 sequenceID, u32 frameOffset, u32 frameCount, ContextStream_s* streamContext )
{
	ScopedStack scopedStack( streamContext->stack );

	Context_s* context = streamContext->stack->newInstance< Context_s >();
	memset( context, 0, sizeof( *context ) );

	context->stream = streamContext;
	context->heap = streamContext->heap;
	context->stack = streamContext->stack;
	context->frames = streamContext->stack->newArray< RawFrame_s >( frameCount );
	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		context->bucketFrames[bucket] = streamContext->stack->newArray< BucketFrame_s >( frameCount );
	context->frameCount = frameCount;

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
			return false;
		}

		V6_MSG( "F%02d: loaded %d blocks from %s.\n", frameRank, context->frames[frameRank].blockCount, filename );
	}

	V6_MSG( "Sorting by key...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank )
	{
		RawFrame_SortByKey( frameRank, context );
		V6_MSG( "F%02d: sorted.\n", frameRank );
	}

#if 0
	V6_MSG( "Bitmapping...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount; ++frameRank )
	{
		RawFrame_GenerateBitmaps( frameRank, context );
		V6_MSG( "F%02d: bitmapped.\n", frameRank );
	}
#endif

	V6_MSG( "Linking...\n" );
	for ( u32 frameRank = 0; frameRank < context->frameCount-1; ++frameRank )
	{
		const u32 linkCount = RawFrame_LinkBlocks( frameRank, context );
		V6_MSG( "F%02d: %8d/%d, %5.1f%% shared block pos.\n", frameRank, linkCount, context->frames[frameRank].blockCount, linkCount * 100.0f / context->frames[frameRank].blockCount );
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
		if ( !RawFrame_Write( frameRank, streamWriter, context ) )
		{
			for( ; frameRank < context->frameCount; ++frameRank )
				RawFrame_Release( frameRank-1, context );
			return false;
		}

		RawFrame_Release( frameRank, context );
		V6_MSG( "F%02d: added %d KB.\n", frameRank, DivKB( streamWriter->GetPos() - prevFileSize ) );
		prevFileSize = streamWriter->GetPos();
	}

	Context_UpdateLimits( context );

	const u32 sequenceSize = streamWriter->GetPos() - prevSequenceSize;
	V6_PRINT( "\n" );
	V6_MSG( "Sequence %04d/%04d: %d KB, avg of %d KB/frame\n", sequenceID, streamContext->desc.sequenceCount, DivKB( sequenceSize ), DivKB( sequenceSize / context->frameCount ) );
	V6_PRINT( "\n" );

	return true;
}


bool VideoStream_Encode( const char* streamFilename, const char* templateRawFilename, u32 frameOffset, u32 frameCount, u32 playRate, IAllocator* heap )
{
	if ( frameCount == 0 || playRate == 0 || playRate > CODEC_FRAME_MAX_COUNT )
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

	Stack stack( heap, 400 * 1024 * 1024 );

	ContextStream_s streamContext = {};
	streamContext.heap = heap;
	streamContext.stack = &stack;
	streamContext.desc.sequenceCount = (frameCount + playRate - 1) / playRate;
	streamContext.desc.frameCount = frameCount;
	streamContext.desc.playRate = playRate;
	streamContext.data.frameOffsets = stack.newArray< u32 >( streamContext.desc.sequenceCount );
	streamContext.data.sequenceByteOffsets = stack.newArray< u32 >( streamContext.desc.sequenceCount );

	memset( streamContext.data.frameOffsets, 0, streamContext.desc.sequenceCount * sizeof( u32 ) );
	memset( streamContext.data.sequenceByteOffsets, 0, streamContext.desc.sequenceCount * sizeof( u32 ) );

	V6_ASSERT( fileWriter.GetPos() == 0 );
	Codec_WriteStream( &fileWriter, &streamContext.desc, &streamContext.data );

	for ( u32 sequenceID = 0; sequenceID < streamContext.desc.sequenceCount; ++sequenceID )
	{
		streamContext.data.frameOffsets[sequenceID] = frameOffset;
		streamContext.data.sequenceByteOffsets[sequenceID] = fileWriter.GetPos();

		const u32 sequenceFrameCount = Min( frameCount, playRate );

		ContextStream_EncodeSequence( &fileWriter, templateRawFilename, sequenceID, frameOffset, sequenceFrameCount, &streamContext );

		frameOffset += sequenceFrameCount;
		frameCount -= sequenceFrameCount;
	}
	V6_ASSERT( frameCount == 0 );

	fileWriter.SetPos( 0 );
	Codec_WriteStream( &fileWriter, &streamContext.desc, &streamContext.data );
	
	const u32 streamSize = fileWriter.GetSize();
	
	V6_PRINT( "\n" );
	V6_MSG( "Stream: %d KB, avg of %d KB/sequence\n", DivKB( streamSize ), DivKB( streamSize / streamContext.desc.sequenceCount ) );
	V6_PRINT( "\n" );

	return true;
}

END_V6_NAMESPACE
