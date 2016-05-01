/*V6*/
#include <v6/core/common.h>

#include <v6/core/bit.h>
#include <v6/core/codec.h>
#include <v6/core/decoder.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_NAMESPACE

static int Block_ComparePos( void* blockPosPointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	u32* blockPos = (u32*)blockPosPointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	return blockPos[blockID0] < blockPos[blockID1] ? -1 : 1;
}

static bool Block_CompareData( const u32* cells0, const u32* cells1, u32 cellCount )
{
	u64 cellPresence0 = 0;
	u64 cellPresence1 = 0;

	u64 cellRGBA0[CODEC_CELL_MAX_COUNT] = {};
	u64 cellRGBA1[CODEC_CELL_MAX_COUNT] = {};

	for ( u32 cellID = 0; cellID < cellCount; ++cellID )
	{
		if ( cells0[cellID] != 0xFFFFFFFF )
		{
			const u32 cellPos = cells0[cellID] & 0xFF;
			cellRGBA0[cellPos] = cells0[cellID];
			cellPresence0 |= 1ll << cellPos;
		}
		if ( cells1[cellID] != 0xFFFFFFFF )
		{
			const u32 cellPos = cells1[cellID] & 0xFF;
			cellRGBA1[cellPos] = cells1[cellID];
			cellPresence1 |= 1ll << cellPos;
		}
	}

	const u64 diffPresence = cellPresence0 ^ cellPresence1;
	if( Bit_GetBitHighCount( diffPresence ) > CODEC_COLOR_COUNT_TOLERANCE )
		return false;

	u64 commonPresence = cellPresence0 & cellPresence1;

	if ( commonPresence == 0 )
		return false;

#if CODEC_COLOR_COMPRESS == 0
	do
	{
		const u32 cellPos = Bit_GetFirstBitHigh( commonPresence );
		commonPresence -= 1ll << cellPos;

		const u32 r0 = (cellRGBA0[cellPos] >> 24) & 0xFF;
		const u32 g0 = (cellRGBA0[cellPos] >> 16) & 0xFF;
		const u32 b0 = (cellRGBA0[cellPos] >>  8) & 0xFF;

		const u32 r1 = (cellRGBA1[cellPos] >> 24) & 0xFF;
		const u32 g1 = (cellRGBA1[cellPos] >> 16) & 0xFF;
		const u32 b1 = (cellRGBA1[cellPos] >>  8) & 0xFF;

		if ( Abs( (int)(r0 - r1) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(g0 - g1) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;
		if ( Abs( (int)(b0 - b1) ) > CODEC_COLOR_ERROR_TOLERANCE )
			return false;

	} while ( commonPresence != 0 );
#endif // #if CODEC_COLOR_COMPRESS == 0

	return true;
}

static bool Sequence_LoadInternal( const char* sequenceFilename, Sequence_s* sequence, IAllocator* allocator, IStack* stack )
{
	CFileReader fileReader;
	if ( !fileReader.Open( sequenceFilename ) )
	{
		V6_ERROR( "Unable to open file %s\n", sequenceFilename );
		return false;
	}

	sequence->buffer = Codec_ReadSequence( &fileReader, &sequence->desc, &sequence->data, allocator );
	if ( !sequence->buffer )
		return false;

	sequence->frameDescArray = allocator->newArray< CodecFrameDesc_s >( sequence->desc.frameCount );
	sequence->frameDataArray = allocator->newArray< CodecFrameData_s >( sequence->desc.frameCount );
	sequence->frameBufferArray = allocator->newArray< void* >( sequence->desc.frameCount );
	memset( sequence->frameBufferArray, 0, sequence->desc.frameCount * sizeof( void* ) );
	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		void* frameBuffer = Codec_ReadFrame( &fileReader, &sequence->frameDescArray[frameID], &sequence->frameDataArray[frameID], frameID, allocator, stack );
		if ( !frameBuffer )
			return false;
		if ( (sequence->frameDescArray[frameID].flags & CODEC_FRAME_FLAG_MOTION) == 0 )
			sequence->frameBufferArray[frameID] = frameBuffer;
	}
	
	if ( fileReader.GetRemaining() > 0 )
	{
		V6_ERROR( "Uncomplete read of file %s\n", sequenceFilename );
		return false;
	}
	
	return true;
}


bool Sequence_LoadDesc( const char* sequenceFilename, CodecSequenceDesc_s* sequenceDesc, IStack* stack )
{
	CFileReader fileReader;
	if ( !fileReader.Open( sequenceFilename ) )
	{
		V6_ERROR( "Unable to open file %s\n", sequenceFilename );
		return false;
	}

	ScopedStack scopedStack( stack );

	CodecSequenceData_s sequenceData;
	return Codec_ReadSequence( &fileReader, sequenceDesc, &sequenceData, stack ) != nullptr;
}

bool Sequence_Load( const char* sequenceFilename, Sequence_s* sequence, IAllocator* allocator, IStack* stack )
{
	memset( sequence, 0, sizeof( *sequence ) );

	if ( !Sequence_LoadInternal( sequenceFilename, sequence, allocator, stack ) )
	{
		Sequence_Release( sequence, allocator );
		return false;
	}

	return true;
}

void Sequence_Release( Sequence_s* sequence, IAllocator* allocator )
{
	allocator->free( sequence->buffer );
	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
		allocator->free( sequence->frameBufferArray[frameID] );
	allocator->deleteArray( sequence->frameDescArray );
	allocator->deleteArray( sequence->frameDataArray );
	allocator->deleteArray( sequence->frameBufferArray );
}

bool Sequence_Validate( const char* templateFilename, const char* sequenceFilename, const Sequence_s* sequence, IAllocator* allocator )
{
	Stack stack( allocator, MulMB( 100 ) );

	const u32 gridMacroMask = (1 << sequence->desc.gridMacroShift) - 1;
	const u32 gridMacroHalfWidth = 1 << (sequence->desc.gridMacroShift-1);
	float gridScales[CODEC_MIP_MAX_COUNT];
	float gridScale = sequence->desc.gridScaleMin;
	for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
		gridScales[mip] = gridScale;

	u32* rangeBlockPosOffsets[CODEC_BUCKET_COUNT];
	u32* rangeBlockDataOffsets[CODEC_BUCKET_COUNT];
	CodecRange_s* rangeDefs[CODEC_BUCKET_COUNT];

	{
		u32 rangeDefOffset = 0;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			rangeDefs[bucket] = sequence->data.rangeDefs + rangeDefOffset;
			const u32 rangeDefCount = sequence->desc.rangeDefCounts[bucket];
			rangeDefOffset += rangeDefCount;
			rangeBlockPosOffsets[bucket] = stack.newArray< u32 >( rangeDefCount );
			rangeBlockDataOffsets[bucket] = stack.newArray< u32 >( rangeDefCount );
		}
	}

	{
		u32 nextRangeIDs[CODEC_BUCKET_COUNT] = {};
		for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
		{
			if ( sequence->frameDescArray[frameID].flags & CODEC_FRAME_FLAG_MOTION )
				continue;

			u32 blockPosOffet = 0;
			u32 blockDataOffet = 0;

			Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
			float gridScale = sequence->desc.gridScaleMin;
			u32 gridMacroHalfWidth = 1 << (sequence->desc.gridMacroShift-1);
			for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
				macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].origin, gridScale, gridMacroHalfWidth ); // patched per frame

			for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; )
			{
				const u32 rangeID = nextRangeIDs[bucket];

				if ( rangeID == sequence->desc.rangeDefCounts[bucket] )
				{
					++bucket;
					continue;
				}

				const CodecRange_s* codecRange = &rangeDefs[bucket][rangeID];
				u32 rangeFrameID = codecRange->frameID8_mip4_blockCount20 >> 24;
				if ( frameID != rangeFrameID )
				{
					++bucket;
					continue;
				}

				const u32 blockCount = codecRange->frameID8_mip4_blockCount20 & 0xFFFFF;
				const u32 mip = (codecRange->frameID8_mip4_blockCount20 >> 20) & 0xF;

				rangeBlockPosOffsets[bucket][rangeID] = blockPosOffet;
				rangeBlockDataOffsets[bucket][rangeID] = blockDataOffet;

				const u32 cellPerBucketCount = 1 << (2 + bucket);
				const u32 dataWidth = cellPerBucketCount;
				blockPosOffet += blockCount;
				blockDataOffet += blockCount * dataWidth;

				++nextRangeIDs[bucket];
			}
		}

		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
			V6_ASSERT( nextRangeIDs[bucket] == sequence->desc.rangeDefCounts[bucket] );
	}

	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		if ( sequence->frameDescArray[frameID].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		ScopedStack scopedStack( &stack );

		char filename[256];
		sprintf_s( filename, sizeof( filename ), templateFilename, frameID );

		CFileReader fileReader;
		if ( !fileReader.Open( filename ) )
		{
			V6_ERROR( "Unable to open %s.\n", filename );
			return false;
		}

		CodecRawFrameDesc_s rawFrameDesc;
		CodecRawFrameData_s rawFrameData;

		if  ( !Codec_ReadRawFrame( &fileReader, &rawFrameDesc, &rawFrameData, &stack ) )
		{
			V6_ERROR( "Unable to read %s.\n", filename );
			return false;
		}

		if ( rawFrameDesc.sampleCount != sequence->desc.sampleCount )
		{
			V6_ERROR( "Incompatible sample count.\n" );
			return false;
		}

		if ( rawFrameDesc.gridMacroShift != sequence->desc.gridMacroShift )
		{
			V6_ERROR( "Incompatible grid resolution.\n" );
			return false;
		}

		if ( rawFrameDesc.gridScaleMin != sequence->desc.gridScaleMin || rawFrameDesc.gridScaleMax != sequence->desc.gridScaleMax )
		{
			V6_ERROR( "Incompatible grid scales.\n" );
			return false;
		}

		u16* rangeIDs = sequence->frameDataArray[frameID].rangeIDs;
		u32* rawFrameBlockPos = (u32*)rawFrameData.blockPos;
		u32* rawFrameBlockData = (u32*)rawFrameData.blockData;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			u32 bucketBlockCount = 0;
			const u32 rangeCount = sequence->frameDescArray[frameID].blockRangeCounts[bucket];
			
			for ( u32 rangeRank = 0; rangeRank < rangeCount; ++rangeRank )
			{
				const u32 rangeID = rangeIDs[rangeRank];
				const CodecRange_s* range = &rangeDefs[bucket][rangeID];
				bucketBlockCount += range->frameID8_mip4_blockCount20 & 0xFFFFF;
			}

			if ( bucketBlockCount != rawFrameDesc.blockCounts[bucket] )
			{
				V6_ERROR( "Incompatible block count.\n" );
				return false;
			}

			const u32 cellPerBucketCount = 1 << (2 + bucket);
			const u32 dataWidth = cellPerBucketCount;

			{
				ScopedStack scopedStackSort( &stack );

				u32* sequenceBlockPos = stack.newArray< u32 >( bucketBlockCount );
				u32* sequenceBlockData = stack.newArray< u32 >( bucketBlockCount * dataWidth );
				u32 sequenceBlockOffset = 0;

				const u32 rangeCount = sequence->frameDescArray[frameID].blockRangeCounts[bucket];
				for ( u32 rangeRank = 0; rangeRank < rangeCount; ++rangeRank )
				{
					const u32 rangeID = rangeIDs[rangeRank];
					const CodecRange_s* range = &rangeDefs[bucket][rangeID];
					const u32 rangeFrameID = range->frameID8_mip4_blockCount20 >> 24;
					const u32 rangeMip = (range->frameID8_mip4_blockCount20 >> 20) & 0xF;
					const u32 rangeBlockCount = range->frameID8_mip4_blockCount20 & 0xFFFFF;
					const u32 frameBlockPosOffset = rangeBlockPosOffsets[bucket][rangeID];
					const u32 frameBlockDataOffset = rangeBlockDataOffsets[bucket][rangeID];
					const Vec3i gridOffset = 
						Codec_ComputeMacroGridCoords( &sequence->frameDescArray[rangeFrameID].origin, gridScales[rangeMip], gridMacroHalfWidth ) -
						Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].origin, gridScales[rangeMip], gridMacroHalfWidth );
					for ( u32 blockRank = 0; blockRank < rangeBlockCount; ++blockRank )
					{
						const u32 sequencePackedBlockPos = sequence->frameDataArray[rangeFrameID].blockPos[frameBlockPosOffset + blockRank];
						const u32 mip = sequencePackedBlockPos >> 28;
						const u32 x = ((sequencePackedBlockPos >> (sequence->desc.gridMacroShift * 0)) & gridMacroMask);
						const u32 y = ((sequencePackedBlockPos >> (sequence->desc.gridMacroShift * 1)) & gridMacroMask);
						const u32 z = ((sequencePackedBlockPos >> (sequence->desc.gridMacroShift * 2)) & gridMacroMask);

						if ( mip != rangeMip )
						{
							V6_ERROR( "Incompatible block mip.\n" );
							return false;
						}

						sequenceBlockPos[sequenceBlockOffset + blockRank] = mip << 28;
						sequenceBlockPos[sequenceBlockOffset + blockRank] |= (x + gridOffset.x) << (sequence->desc.gridMacroShift * 0);
						sequenceBlockPos[sequenceBlockOffset + blockRank] |= (y + gridOffset.y) << (sequence->desc.gridMacroShift * 1);
						sequenceBlockPos[sequenceBlockOffset + blockRank] |= (z + gridOffset.z) << (sequence->desc.gridMacroShift * 2);
					}
					memcpy( sequenceBlockData + sequenceBlockOffset * dataWidth, sequence->frameDataArray[rangeFrameID].blockData + frameBlockDataOffset, rangeBlockCount * dataWidth * 4 );

					sequenceBlockOffset += rangeBlockCount;
				}
				
				u32* sequenceBlockIDs = stack.newArray< u32 >( bucketBlockCount );
				for ( u32 blockRank = 0; blockRank < bucketBlockCount; ++blockRank )
					sequenceBlockIDs[blockRank] = blockRank;

				qsort_s( sequenceBlockIDs, bucketBlockCount, sizeof( u32 ), Block_ComparePos, sequenceBlockPos );

				u32* rawFrameBlockIDs = stack.newArray< u32 >( bucketBlockCount );
				for ( u32 blockRank = 0; blockRank < bucketBlockCount; ++blockRank )
					rawFrameBlockIDs[blockRank] = blockRank;

				qsort_s( rawFrameBlockIDs, bucketBlockCount, sizeof( u32 ), Block_ComparePos, rawFrameBlockPos );

				for ( u32 blockRank = 0; blockRank < bucketBlockCount; ++blockRank )
				{
					const u32 sequenceBlockID = sequenceBlockIDs[blockRank];
					const u32 rawFrameBlockID = rawFrameBlockIDs[blockRank];
					
					if ( sequenceBlockPos[sequenceBlockID] != rawFrameBlockPos[rawFrameBlockID] )
					{
						V6_ERROR( "Incompatible block pos.\n" );
						return false;
					}

					if ( !Block_CompareData( sequenceBlockData + sequenceBlockID * dataWidth, rawFrameBlockData + rawFrameBlockID * cellPerBucketCount, cellPerBucketCount ) )
					{
						V6_ERROR( "Incompatible block data.\n" );
						return false;
					}
				}
			}

			rangeIDs += rangeCount;
			rawFrameBlockPos += bucketBlockCount;
			rawFrameBlockData += bucketBlockCount * cellPerBucketCount;
		}

		V6_MSG( "F%02d: validated.\n", frameID );
	}

	return true;
}

END_V6_NAMESPACE
