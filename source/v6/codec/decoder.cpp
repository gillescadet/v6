/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/codec/decoder.h>
#include <v6/core/bit.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_NAMESPACE

struct DecoderBlock_s
{
	u32			packedBlockPos;
	u32			cellRGBA[CODEC_CELL_MAX_COUNT];
	u32			cellCount;
};

static int Block_ComparePos( void* blockPointer, void const* blockIDPointer0, void const* blockIDPointer1 )
{
	DecoderBlock_s* blocks = (DecoderBlock_s*)blockPointer;
	const u32 blockID0 = *((u32*)blockIDPointer0);
	const u32 blockID1 = *((u32*)blockIDPointer1);

	return blocks[blockID0].packedBlockPos < blocks[blockID1].packedBlockPos ? -1 : 1;
}

static bool Block_CompareData( const DecoderBlock_s* rawBlock, const DecoderBlock_s* sequenceBlock )
{
	u64 cellPresence0 = 0;
	for ( u32 cellRank = 0; cellRank < rawBlock->cellCount; ++cellRank )
	{
		const u32 cellID = rawBlock->cellRGBA[cellRank] & 0xFF;
		cellPresence0 |= 1ll << cellID;
	}

	u64 cellPresence1 = 0;
	u32 cellRGBA[64];
	for ( u32 cellRank = 0; cellRank < sequenceBlock->cellCount; ++cellRank )
	{
		const u32 cellID = sequenceBlock->cellRGBA[cellRank] & 0xFF;
		cellPresence1 |= 1ll << cellID;
		cellRGBA[cellID] = sequenceBlock->cellRGBA[cellRank];
	}

	const u64 diffPresence = cellPresence0 ^ cellPresence1;
	if( Bit_GetBitHighCount( diffPresence ) > CODEC_COLOR_COUNT_TOLERANCE )
		return false;

	const u64 commonPresence = cellPresence0 & cellPresence1;
	if ( commonPresence == 0 )
		return false;

	for ( u32 cellRank = 0; cellRank < rawBlock->cellCount; ++cellRank )
	{
		const u32 cellID = rawBlock->cellRGBA[cellRank] & 0xFF;
		if ( commonPresence & (1ll << cellID ) )
		{

			const u32 refR = (rawBlock->cellRGBA[cellRank] >> 24) & 0xFF;
			const u32 refG = (rawBlock->cellRGBA[cellRank] >> 16) & 0xFF;
			const u32 refB = (rawBlock->cellRGBA[cellRank] >>  8) & 0xFF;

			const u32 codR = (cellRGBA[cellID] >> 24) & 0xFF;
			const u32 codG = (cellRGBA[cellID] >> 16) & 0xFF;
			const u32 codB = (cellRGBA[cellID] >>  8) & 0xFF;

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
	if ( !Codec_ReadStreamDesc( streamReader, &stream->desc ) )
		return false;

	stream->sequences = allocator->newArray< VideoSequence_s >( stream->desc.sequenceCount );
	memset( stream->sequences, 0, stream->desc.sequenceCount * sizeof( *stream->sequences ) );
	
	stream->frameOffets = allocator->newArray< u32 >( stream->desc.sequenceCount );
	u32 frameOffset = 0;
	
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		if ( !VideoSequence_LoadInternal( &stream->sequences[sequenceID], streamReader, sequenceID, allocator, stack ) )
			return false;

		stream->frameOffets[sequenceID] = frameOffset;
		frameOffset += stream->sequences[sequenceID].desc.frameCount;
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
#if 1
	u32 frameID = frameOffset;
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		Stack stack( allocator, MulMB( 2000 ) );

		const VideoSequence_s* sequence = &stream->sequences[sequenceID];

		const u32 gridMacroHalfWidth = stream->desc.gridWidth >> 3;
		float gridScales[CODEC_MIP_MAX_COUNT];
		float gridScale = stream->desc.gridScaleMin;
		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
			gridScales[mip] = gridScale;

		{
			u32 nextRangeID = 0;
			for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
			{
				if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
					continue;

				u32 blockPosOffet = 0;
				for (;;)
				{
					const u32 rangeID = nextRangeID;

					if ( rangeID == sequence->desc.rangeDefCount  )
						break;

					const CodecRange_s* codecRange = &sequence->data.rangeDefs[rangeID];
					const u32 rangeFrameRank = codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 25;
					if ( frameRank != rangeFrameRank )
						break;

					++nextRangeID;
				}
			}

			V6_ASSERT( nextRangeID == sequence->desc.rangeDefCount );
		}

		for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank, ++frameID )
		{
			if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
				continue;

			ScopedStack scopedStack( &stack );

			char filename[256];
			sprintf_s( filename, sizeof( filename ), templateFilename, frameID );

			CFileReader fileReader;
			if ( !fileReader.Open( filename, FILE_OPEN_FLAG_UNBUFFERED ) )
			{
				V6_ERROR( "Unable to open %s.\n", filename );
				return false;
			}

			CodecRawFrameDesc_s rawFrameDesc;
			CodecRawFrameData_s rawFrameData;

			if  ( !Codec_ReadRawFrame( &fileReader, &rawFrameDesc, &rawFrameData, nullptr, &stack ) )
			{
				V6_ERROR( "Unable to read %s.\n", filename );
				return false;
			}

			if ( rawFrameDesc.frameRate != stream->desc.frameRate )
			{
				V6_ERROR( "Incompatible frame rate.\n" );
				return false;
			}

			if ( rawFrameDesc.gridWidth != stream->desc.gridWidth )
			{
				V6_ERROR( "Incompatible grid resolution.\n" );
				return false;
			}

			if ( rawFrameDesc.gridScaleMin != stream->desc.gridScaleMin || rawFrameDesc.gridScaleMax != stream->desc.gridScaleMax )
			{
				V6_ERROR( "Incompatible grid scales.\n" );
				return false;
			}

			if ( rawFrameDesc.flags != stream->desc.flags )
			{
				V6_ERROR( "Incompatible flags.\n" );
				return false;
			}

			u32 blockCount = 0;
			u32 badBlockDataCount = 0;
			for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
				blockCount += rawFrameDesc.blockCounts[bucket];

			if ( sequence->frameDescArray[frameRank].blockCount != blockCount )
			{
				V6_ERROR( "Incompatible block count.\n" );
				return false;
			}


			DecoderBlock_s* rawBlocks = stack.newArray< DecoderBlock_s >( blockCount );

			{
				// Load raw blocks
				
				u32* rawFrameBlockPos = (u32*)rawFrameData.blockPos;
				u32* rawFrameBlockData = (u32*)rawFrameData.blockData;
			
				u32 rawFrameBlockID = 0;
				u32 rawFrameBlockDataOffset = 0;
				for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
				{
					const u32 cellPerBucketCount = 1 << (2 + bucket);

					for ( u32 blockRank = 0; blockRank < rawFrameDesc.blockCounts[bucket]; ++blockRank )
					{
						DecoderBlock_s* rawBlock = &rawBlocks[rawFrameBlockID];
						rawBlock->packedBlockPos = rawFrameBlockPos[rawFrameBlockID];
						rawBlock->cellCount = 0;
						for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
						{
							const u32 rgba = rawFrameBlockData[rawFrameBlockDataOffset + cellID];
							if ( rgba == 0xFFFFFFFF )
								continue;
							rawBlock->cellRGBA[cellID] = rgba;
							++rawBlock->cellCount;
						}

						++rawFrameBlockID;
						rawFrameBlockDataOffset += cellPerBucketCount;
					}
				}
			}

			DecoderBlock_s* sequenceBlocks = stack.newArray< DecoderBlock_s >( blockCount );

			// Load sequence blocks
			{
				u32 blockOffset = 0;

				for ( u32 rangeRank = 0; rangeRank < sequence->frameDescArray[frameRank].blockRangeCount; ++rangeRank )
				{
					const u32 rangeID = sequence->frameDataArray[frameRank].rangeIDs[rangeRank];
					const CodecRange_s* range = &sequence->data.rangeDefs[rangeID];
					const u32 rangeFrameRank = range->frameRank7_newBlock1_grid4_blockCount20 >> 25;
					const u32 rangeGrid = (range->frameRank7_newBlock1_grid4_blockCount20 >> 20) & 0xF;
					const u32 rangeBlockCount = range->frameRank7_newBlock1_grid4_blockCount20 & 0xFFFFF;
					Vec3i gridOffset = Vec3i_Zero();
					if ( stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
					{
						gridOffset =
							Codec_ComputeMacroGridCoords( &sequence->frameDescArray[rangeFrameRank].gridOrigin, gridScales[rangeGrid], gridMacroHalfWidth ) -
							Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameRank].gridOrigin, gridScales[rangeGrid], gridMacroHalfWidth );
					}

					for ( u32 blockRank = 0; blockRank < rangeBlockCount; ++blockRank )
					{
						const u32 blockID = blockOffset + blockRank;
						const u32 sequencePackedBlockPos = sequence->frameDataArray[rangeFrameRank].blockPos[blockID];
						DecoderBlock_s* sequenceBlock = &sequenceBlocks[blockID];

						if ( stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
						{
							const u32 mip = sequencePackedBlockPos >> 28;
							const u32 x = ((sequencePackedBlockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 0)) & CODEC_MIP_MACRO_XYZ_BIT_MASK);
							const u32 y = ((sequencePackedBlockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 1)) & CODEC_MIP_MACRO_XYZ_BIT_MASK);
							const u32 z = ((sequencePackedBlockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 2)) & CODEC_MIP_MACRO_XYZ_BIT_MASK);

							if ( mip != rangeGrid )
							{
								V6_ERROR( "Incompatible block mip.\n" );
								return false;
							}

							sequenceBlock->packedBlockPos = mip << 28;
							sequenceBlock->packedBlockPos |= (x + gridOffset.x) << (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 0);
							sequenceBlock->packedBlockPos |= (y + gridOffset.y) << (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 1);
							sequenceBlock->packedBlockPos |= (z + gridOffset.z) << (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 2);
						}
						else
						{
							const u32 face = sequencePackedBlockPos >> 29;

							if ( face != rangeGrid )
							{
								V6_ERROR( "Incompatible block face.\n" );
								return false;
							}

							sequenceBlock->packedBlockPos = sequencePackedBlockPos;
						}
							
						EncodedBlockEx_s encodedBlock;
						encodedBlock.cellEndColors = sequence->frameDataArray[rangeFrameRank].blockCellEndColors[blockID];
						encodedBlock.cellPresence = sequence->frameDataArray[rangeFrameRank].blockCellPresences0[blockID] | ((u64)sequence->frameDataArray[rangeFrameRank].blockCellPresences1[blockID] << 32);
						encodedBlock.cellColorIndices[0] = sequence->frameDataArray[rangeFrameRank].blockCellColorIndices0[blockID] | ((u64)sequence->frameDataArray[rangeFrameRank].blockCellColorIndices1[blockID] << 32);
						encodedBlock.cellColorIndices[1] = sequence->frameDataArray[rangeFrameRank].blockCellColorIndices2[blockID] | ((u64)sequence->frameDataArray[rangeFrameRank].blockCellColorIndices3[blockID] << 32);

						Block_Decode( sequenceBlock->cellRGBA, &sequenceBlock->cellCount, &encodedBlock );
					}

					blockOffset += rangeBlockCount;
				}
					
				u32* rawFrameBlockIDs = stack.newArray< u32 >( blockCount );
				for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
					rawFrameBlockIDs[blockRank] = blockRank;

				qsort_s( rawFrameBlockIDs, blockCount, sizeof( u32 ), Block_ComparePos, rawBlocks );

				u32* sequenceBlockIDs = stack.newArray< u32 >( blockCount );
				for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
					sequenceBlockIDs[blockRank] = blockRank;

				qsort_s( sequenceBlockIDs, blockCount, sizeof( u32 ), Block_ComparePos, sequenceBlocks );

				for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
				{
					const u32 rawFrameBlockID = rawFrameBlockIDs[blockRank];
					const u32 sequenceBlockID = sequenceBlockIDs[blockRank];

					const DecoderBlock_s* rawBlock = &rawBlocks[rawFrameBlockID];
					const DecoderBlock_s* sequenceBlock = &sequenceBlocks[sequenceBlockID];

					if ( rawBlock->packedBlockPos != sequenceBlock->packedBlockPos )
					{
						V6_ERROR( "Incompatible block pos.\n" );
						return false;
					}

#if 1
					if ( !Block_CompareData( &rawBlocks[rawFrameBlockID], &sequenceBlocks[sequenceBlockID] ) )
						++badBlockDataCount;
#endif
				}
			}

			if ( badBlockDataCount )
				V6_MSG( "S%04d_F%02d: validated with %d/%d bad block data.\n", sequenceID, frameRank, badBlockDataCount, blockCount );
			else
				V6_MSG( "S%04d_F%02d: validated.\n", sequenceID, frameRank );
		}
	}
#endif
	return true;
}

bool VideoStream_Load( VideoStream_s* stream, const char* streamFilename, IAllocator* allocator, IStack* stack )
{
	memset( stream, 0, sizeof( *stream ) );

	CFileReader fileReader;
	if ( !fileReader.Open( streamFilename, 0 ) )
	{
		V6_ERROR( "Unable to open file %s\n", streamFilename );
		return false;
	}

	if ( !VideoStream_LoadInternal( stream, &fileReader, allocator, stack ) )
	{
		VideoStream_Release( stream, allocator );
		return false;
	}

	V6_ASSERT(stream->desc.sequenceCount > 0 );
	V6_ASSERT(stream->desc.frameCount > 0 );

	strcpy_s( stream->name, sizeof( stream->name ), streamFilename );

	return true;
}

bool VideoStream_LoadDesc( const char* streamFilename, CodecStreamDesc_s* streamDesc )
{
	CFileReader fileReader;
	if ( !fileReader.Open( streamFilename, 0 ) )
	{
		V6_ERROR( "Unable to open file %s\n", streamFilename );
		return false;
	}

	return Codec_ReadStreamDesc( &fileReader, streamDesc );
}

void VideoStream_Release( VideoStream_s* stream, IAllocator* allocator )
{
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
		VideoSequence_Release( &stream->sequences[sequenceID], allocator );
	allocator->deleteArray( stream->sequences );
	allocator->deleteArray( stream->frameOffets );
	memset( stream, 0, sizeof( *stream ) );
}

u32 VideoStream_FindSequenceIDFromFrameID( const VideoStream_s* stream, u32 frameID )
{
	V6_ASSERT( frameID < stream->desc.frameCount );

	u32 minSequenceID = 0;
	u32 maxSequenceID = stream->desc.sequenceCount;

	while ( maxSequenceID - minSequenceID > 1 )
	{
		const u32 midSequenceID = (minSequenceID + maxSequenceID) / 2;
		if ( frameID < stream->frameOffets[midSequenceID] )
			maxSequenceID = midSequenceID;
		else
			minSequenceID = midSequenceID;
	}
	
	return minSequenceID;
}

END_V6_NAMESPACE
