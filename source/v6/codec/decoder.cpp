/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/codec/decoder.h>
#include <v6/core/bit.h>
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

static bool Block_CompareData( const EncodedBlockEx_s* encodedBlock, const u32* rawCells, u32 cellCount1 )
{
	u64 cellPresence1 = 0;

	u32 cellRGBA1[64];
	for ( u32 cellRank = 0; cellRank < cellCount1; ++cellRank )
	{
		if ( rawCells[cellRank] != 0xFFFFFFFF )
		{
			const u32 cellID = rawCells[cellRank] & 0xFF;
			cellPresence1 |= 1ll << cellID;

			cellRGBA1[cellID] = rawCells[cellRank];
		}
	}

	const u64 diffPresence = encodedBlock->cellPresence ^ cellPresence1;
	if( Bit_GetBitHighCount( diffPresence ) > CODEC_COLOR_COUNT_TOLERANCE )
		return false;

	u64 commonPresence = encodedBlock->cellPresence & cellPresence1;

	if ( commonPresence == 0 )
		return false;

	u32 cellRGBA0[64];
	u32 cellCount0;
	Block_Decode( cellRGBA0, &cellCount0, encodedBlock );

	for ( u32 cellRank = 0; cellRank < cellCount0; ++cellRank )
	{
		const u32 cellID = cellRGBA0[cellRank] & 0xFF;
		if ( commonPresence & (1ll << cellID ) )
		{
#if 0
			{
				const u32 blockCellPresenceLow = encodedBlock->cellPresence & 0xFFFFFFFF;
				const u32 blockCellPresenceHigh = (encodedBlock->cellPresence >> 32) & 0xFFFFFFFF;
				const u32 cellMask = (1u << (cellID & 31)) - 1;
				const u32 cellLowRank = Bit_GetBitHighCount( blockCellPresenceLow & cellMask );
				const u32 cellHighRank = Bit_GetBitHighCount( blockCellPresenceLow ) + Bit_GetBitHighCount( blockCellPresenceHigh & cellMask );
				const u32 cellCheckRank = cellID < 32 ? cellLowRank : cellHighRank;
				V6_ASSERT( cellCheckRank == cellRank );
			}
#endif

			const u32 refR = (cellRGBA1[cellID] >> 24) & 0xFF;
			const u32 refG = (cellRGBA1[cellID] >> 16) & 0xFF;
			const u32 refB = (cellRGBA1[cellID] >>  8) & 0xFF;

			const u32 codR = (cellRGBA0[cellRank] >> 24) & 0xFF;
			const u32 codG = (cellRGBA0[cellRank] >> 16) & 0xFF;
			const u32 codB = (cellRGBA0[cellRank] >>  8) & 0xFF;

			const u32 tolerance = CODEC_COLOR_ERROR_TOLERANCE * 2; // approximate BC1 compression error

			if ( Abs( (int)(refR - codR) ) > tolerance )
				return false;

			if ( Abs( (int)(refG - codG) ) > tolerance )
				return false;

			if ( Abs( (int)(refB - codB) ) > tolerance )
				return false;
		}
	}

	return true;
}

static bool VideoSequence_LoadInternal( VideoSequence_s* sequence, IStreamReader* streamReader, u32 sequenceID, IAllocator* allocator, IStack* stack )
{
	sequence->buffer = Codec_ReadSequence( streamReader, &sequence->desc, &sequence->data, sequenceID, allocator );
	if ( !sequence->buffer )
		return false;

	sequence->frameDescArray = allocator->newArray< CodecFrameDesc_s >( sequence->desc.frameCount );
	sequence->frameDataArray = allocator->newArray< CodecFrameData_s >( sequence->desc.frameCount );
	sequence->frameBufferArray = allocator->newArray< void* >( sequence->desc.frameCount );
	memset( sequence->frameBufferArray, 0, sequence->desc.frameCount * sizeof( void* ) );
	for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
	{
		void* frameBuffer = Codec_ReadFrame( streamReader, &sequence->frameDescArray[frameRank], &sequence->frameDataArray[frameRank], frameRank, allocator, stack );
		if ( !frameBuffer )
			return false;
		if ( (sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION) == 0 )
			sequence->frameBufferArray[frameRank] = frameBuffer;
	}
	
	return true;
}

static bool VideoStream_LoadInternal( VideoStream_s* stream, IStreamReader* streamReader, IAllocator* allocator, IStack* stack )
{
	stream->buffer = Codec_ReadStream( streamReader, &stream->desc, &stream->data, allocator );
	if ( !stream->buffer )
		return false;

	stream->sequences = allocator->newArray< VideoSequence_s >( stream->desc.sequenceCount );
	memset( stream->sequences, 0, stream->desc.sequenceCount * sizeof( *stream->sequences ) );
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		if ( !VideoSequence_LoadInternal( &stream->sequences[sequenceID], streamReader, sequenceID, allocator, stack ) )
			return false;
	}
	
	return true;
}

bool VideoSequence_Load( VideoSequence_s* sequence, IStreamReader* streamReader, u32 sequenceID, IAllocator* allocator, IStack* stack )
{
	memset( sequence, 0, sizeof( *sequence ) );

	if ( !VideoSequence_LoadInternal( sequence, streamReader, sequenceID, allocator, stack ) )
	{
		VideoSequence_Release( sequence, allocator );
		return false;
	}

	return true;
}

void VideoSequence_Release( VideoSequence_s* sequence, IAllocator* allocator )
{
	allocator->free( sequence->buffer );
	sequence->buffer = nullptr;
	for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
		allocator->free( sequence->frameBufferArray[frameRank] );
	allocator->deleteArray( sequence->frameDescArray );
	allocator->deleteArray( sequence->frameDataArray );
	allocator->deleteArray( sequence->frameBufferArray );
}

bool VideoStream_Validate( const VideoStream_s* stream, const char* templateFilename, u32 frameOffset, IAllocator* allocator )
{
	u32 frameID = frameOffset;
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		Stack stack( allocator, MulMB( 100 ) );

		const VideoSequence_s* sequence = &stream->sequences[sequenceID];

		const u32 gridMacroMask = (1 << stream->desc.gridMacroShift) - 1;
		const u32 gridMacroHalfWidth = 1 << (stream->desc.gridMacroShift-1);
		float gridScales[CODEC_MIP_MAX_COUNT];
		float gridScale = stream->desc.gridScaleMin;
		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
			gridScales[mip] = gridScale;

		u32* rangeBlockPosOffsets[CODEC_BUCKET_COUNT];
		CodecRange_s* rangeDefs[CODEC_BUCKET_COUNT];

		{
			u32 rangeDefOffset = 0;
			for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
			{
				rangeDefs[bucket] = sequence->data.rangeDefs + rangeDefOffset;
				const u32 rangeDefCount = sequence->desc.rangeDefCounts[bucket];
				rangeDefOffset += rangeDefCount;
				rangeBlockPosOffsets[bucket] = stack.newArray< u32 >( rangeDefCount );
			}
		}

		{
			u32 nextRangeIDs[CODEC_BUCKET_COUNT] = {};
			for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
			{
				if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
					continue;

				u32 blockPosOffet = 0;

				Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
				float gridScale = stream->desc.gridScaleMin;
				const u32 gridMacroHalfWidth = 1 << (stream->desc.gridMacroShift-1);
				for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
					macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameRank].gridOrigin, gridScale, gridMacroHalfWidth ); // patched per frame

				for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; )
				{
					const u32 rangeID = nextRangeIDs[bucket];

					if ( rangeID == sequence->desc.rangeDefCounts[bucket] )
					{
						++bucket;
						continue;
					}

					const CodecRange_s* codecRange = &rangeDefs[bucket][rangeID];
					u32 rangeFrameRank = codecRange->frameRank8_mip4_blockCount20 >> 24;
					if ( frameRank != rangeFrameRank )
					{
						++bucket;
						continue;
					}

					const u32 blockCount = codecRange->frameRank8_mip4_blockCount20 & 0xFFFFF;
					const u32 mip = (codecRange->frameRank8_mip4_blockCount20 >> 20) & 0xF;

					rangeBlockPosOffsets[bucket][rangeID] = blockPosOffet;

					const u32 cellPerBucketCount = 1 << (2 + bucket);
					const u32 dataWidth = cellPerBucketCount;
					blockPosOffet += blockCount;

					++nextRangeIDs[bucket];
				}
			}

			for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
				V6_ASSERT( nextRangeIDs[bucket] == sequence->desc.rangeDefCounts[bucket] );
		}

		for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank, ++frameID )
		{
			if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
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

			if ( rawFrameDesc.frameRate != stream->desc.frameRate )
			{
				V6_ERROR( "Incompatible frame rate.\n" );
				return false;
			}

			if ( rawFrameDesc.sampleCount != stream->desc.sampleCount )
			{
				V6_ERROR( "Incompatible sample count.\n" );
				return false;
			}

			if ( rawFrameDesc.gridMacroShift != stream->desc.gridMacroShift )
			{
				V6_ERROR( "Incompatible grid resolution.\n" );
				return false;
			}

			if ( rawFrameDesc.gridScaleMin != stream->desc.gridScaleMin || rawFrameDesc.gridScaleMax != stream->desc.gridScaleMax )
			{
				V6_ERROR( "Incompatible grid scales.\n" );
				return false;
			}

			u16* rangeIDs = sequence->frameDataArray[frameRank].rangeIDs;
			u32* rawFrameBlockPos = (u32*)rawFrameData.blockPos;
			u32* rawFrameBlockData = (u32*)rawFrameData.blockData;
			for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
			{
				u32 bucketBlockCount = 0;
				const u32 rangeCount = sequence->frameDescArray[frameRank].blockRangeCounts[bucket];
				
				for ( u32 rangeRank = 0; rangeRank < rangeCount; ++rangeRank )
				{
					const u32 rangeID = rangeIDs[rangeRank];
					const CodecRange_s* range = &rangeDefs[bucket][rangeID];
					bucketBlockCount += range->frameRank8_mip4_blockCount20 & 0xFFFFF;
				}

				if ( bucketBlockCount != rawFrameDesc.blockCounts[bucket] )
				{
					V6_ERROR( "Incompatible block count.\n" );
					return false;
				}

				const u32 cellPerBucketCount = 1 << (2 + bucket);

				{
					ScopedStack scopedStackSort( &stack );

					u32* sequenceBlockPos = stack.newArray< u32 >( bucketBlockCount );
					u64* sequenceBlockCellPresences = stack.newArray< u64 >( bucketBlockCount );
					u32* sequenceBlockCellEndColors = stack.newArray< u32 >( bucketBlockCount );
					u64* sequenceBlockCellColorIndices0 = stack.newArray< u64 >( bucketBlockCount );
					u64* sequenceBlockCellColorIndices1 = stack.newArray< u64 >( bucketBlockCount );
					u32 sequenceBlockOffset = 0;

					const u32 rangeCount = sequence->frameDescArray[frameRank].blockRangeCounts[bucket];
					for ( u32 rangeRank = 0; rangeRank < rangeCount; ++rangeRank )
					{
						const u32 rangeID = rangeIDs[rangeRank];
						const CodecRange_s* range = &rangeDefs[bucket][rangeID];
						const u32 rangeFrameRank = range->frameRank8_mip4_blockCount20 >> 24;
						const u32 rangeMip = (range->frameRank8_mip4_blockCount20 >> 20) & 0xF;
						const u32 rangeBlockCount = range->frameRank8_mip4_blockCount20 & 0xFFFFF;
						const u32 frameBlockPosOffset = rangeBlockPosOffsets[bucket][rangeID];
						const Vec3i gridOffset = 
							Codec_ComputeMacroGridCoords( &sequence->frameDescArray[rangeFrameRank].gridOrigin, gridScales[rangeMip], gridMacroHalfWidth ) -
							Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameRank].gridOrigin, gridScales[rangeMip], gridMacroHalfWidth );
						for ( u32 blockRank = 0; blockRank < rangeBlockCount; ++blockRank )
						{
							const u32 sequencePackedBlockPos = sequence->frameDataArray[rangeFrameRank].blockPos[frameBlockPosOffset + blockRank];
							const u32 mip = sequencePackedBlockPos >> 28;
							const u32 x = ((sequencePackedBlockPos >> (stream->desc.gridMacroShift * 0)) & gridMacroMask);
							const u32 y = ((sequencePackedBlockPos >> (stream->desc.gridMacroShift * 1)) & gridMacroMask);
							const u32 z = ((sequencePackedBlockPos >> (stream->desc.gridMacroShift * 2)) & gridMacroMask);

							if ( mip != rangeMip )
							{
								V6_ERROR( "Incompatible block mip.\n" );
								return false;
							}

							sequenceBlockPos[sequenceBlockOffset + blockRank] = mip << 28;
							sequenceBlockPos[sequenceBlockOffset + blockRank] |= (x + gridOffset.x) << (stream->desc.gridMacroShift * 0);
							sequenceBlockPos[sequenceBlockOffset + blockRank] |= (y + gridOffset.y) << (stream->desc.gridMacroShift * 1);
							sequenceBlockPos[sequenceBlockOffset + blockRank] |= (z + gridOffset.z) << (stream->desc.gridMacroShift * 2);
						}
						memcpy( sequenceBlockCellPresences + sequenceBlockOffset, sequence->frameDataArray[rangeFrameRank].blockCellPresences + frameBlockPosOffset, rangeBlockCount * 8 );
						memcpy( sequenceBlockCellEndColors + sequenceBlockOffset, sequence->frameDataArray[rangeFrameRank].blockCellEndColors + frameBlockPosOffset, rangeBlockCount * 4 );
						memcpy( sequenceBlockCellColorIndices0 + sequenceBlockOffset, sequence->frameDataArray[rangeFrameRank].blockCellColorIndices0 + frameBlockPosOffset, rangeBlockCount * 8 );
						memcpy( sequenceBlockCellColorIndices1 + sequenceBlockOffset, sequence->frameDataArray[rangeFrameRank].blockCellColorIndices1 + frameBlockPosOffset, rangeBlockCount * 8 );

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

						EncodedBlockEx_s encodedBlock;
						encodedBlock.cellPresence = sequenceBlockCellPresences[sequenceBlockID];
						encodedBlock.cellEndColors = sequenceBlockCellEndColors[sequenceBlockID];
						encodedBlock.cellColorIndices[0] = sequenceBlockCellColorIndices0[sequenceBlockID];
						encodedBlock.cellColorIndices[1] = sequenceBlockCellColorIndices1[sequenceBlockID];
						if ( !Block_CompareData( &encodedBlock, rawFrameBlockData + rawFrameBlockID * cellPerBucketCount, cellPerBucketCount ) )
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

			V6_MSG( "S%04d_F%02d: validated.\n", sequenceID, frameRank );
		}
	}

	return true;
}

bool VideoStream_Load( VideoStream_s* stream, const char* streamFilename, IAllocator* allocator, IStack* stack )
{
	memset( stream, 0, sizeof( *stream ) );

	CFileReader fileReader;
	if ( !fileReader.Open( streamFilename ) )
	{
		V6_ERROR( "Unable to open file %s\n", streamFilename );
		return false;
	}

	if ( !VideoStream_LoadInternal( stream, &fileReader, allocator, stack ) )
	{
		VideoStream_Release( stream, allocator );
		return false;
	}

	strcpy_s( stream->name, sizeof( stream->name ), streamFilename );

	return true;
}

bool VideoStream_LoadDesc( const char* streamFilename, CodecStreamDesc_s* streamDesc, IStack* stack )
{
	CFileReader fileReader;
	if ( !fileReader.Open( streamFilename ) )
	{
		V6_ERROR( "Unable to open file %s\n", streamFilename );
		return false;
	}

	ScopedStack scopedStack( stack );

	CodecStreamData_s streamData;
	return Codec_ReadStream( &fileReader, streamDesc, &streamData, stack ) != nullptr;
}

void VideoStream_Release( VideoStream_s* stream, IAllocator* allocator )
{
	allocator->free( stream->buffer );
	stream->buffer = nullptr;
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
		VideoSequence_Release( &stream->sequences[sequenceID], allocator );
	allocator->deleteArray( stream->sequences );
}

END_V6_NAMESPACE
