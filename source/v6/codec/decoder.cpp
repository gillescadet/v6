/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/codec/decoder.h>
#include <v6/core/bit.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>

#if CODEC_DEBUG_SIMULATE_STREAM_ERROR == 1
#pragma message( "### CODEC DEBUG SIMULATE STREAM ERROR ENABLED ###" )
#endif

#if CODEC_CHECK_PERFORMANCE == 1
#pragma message( "### CODEC CHECK PERFORMANCE ENABLED ###" )
#endif

#define VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT 0xFFFFFFFF

BEGIN_V6_NAMESPACE

struct VideoFrameJob_s
{
	IAllocator*					heap;
	IStack*						stack;
	VideoStreamPrefetcher_s*	prefetcher;
	const void*					frameData;
	u32							frameDataSize;
	u32							sequenceID;
	u32							frameRank;
};

#if CODEC_CHECK_PERFORMANCE  == 1
#define CODEC_SCOPED_HITCH_DETECTION( NAME, MIN_TIME_US ) SCOPED_HITCH_DETECTION( NAME, MIN_TIME_US )
#else
#define CODEC_SCOPED_HITCH_DETECTION( NAME, MIN_TIME_US )
#endif // #if CODEC_CHECK_PERFORMANCE  == 1

struct DecoderBlock_s
{
	u32			packedBlockPos;
	u32			cellRGBA[CODEC_CELL_MAX_COUNT];
	u32			cellCount;
};

static const CPUEventID_t s_cpuEventDecode		= CPUEvent_Register( "Decode", true );

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

static bool VideoSequence_LoadInternal( VideoSequence_s* sequence, IStreamReader* streamReader, IAllocator* allocator, IStack* stack )
{
	if ( !Codec_ReadSequenceDesc( streamReader, &sequence->desc ) )
		return false;

	V6_ALIGN( CODEC_CLUSTER_SIZE ) u8 streamReaderBuffer[CODEC_CLUSTER_SIZE];
	CStreamReaderWithBuffering streamReaderWithBuffering( streamReader, streamReaderBuffer, sizeof( streamReaderBuffer ) );

	memset( sequence->frameBufferArray, 0, sequence->desc.frameCount * sizeof( void* ) );
	for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
	{
		void* frameBuffer;
		Codec_ReadFrame( &frameBuffer, &streamReaderWithBuffering, &sequence->frameDescArray[frameRank], &sequence->frameDataArray[frameRank], frameRank, allocator, stack );
		if ( !frameBuffer )
		{
			streamReaderWithBuffering.SkipUnreadBuffer();
			return false;
		}
		if ( (sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION) == 0 )
			sequence->frameBufferArray[frameRank] = frameBuffer;
	}

	streamReaderWithBuffering.SkipUnreadBuffer();
	
	return true;
}

static bool VideoStream_LoadInternal( VideoStream_s* stream, IStreamReader* streamReader, IAllocator* allocator, IStack* stack )
{
	stream->buffer = Codec_ReadStreamDesc( streamReader, &stream->desc, &stream->data, allocator );
	if ( !stream->buffer )
		return false;

	stream->sequences = allocator->newArray< VideoSequence_s >( stream->desc.sequenceCount, "VideoStreamSequences" );
	memset( stream->sequences, 0, stream->desc.sequenceCount * sizeof( *stream->sequences ) );
	
	stream->frameOffsets = allocator->newArray< u32 >( stream->desc.sequenceCount+1, "VideoStreamFrameOffsets" );
	u32 frameOffset = 0;
	
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		V6_DEVMSG( "Loading sequence %d/%d...\n", sequenceID+1, stream->desc.sequenceCount );

		if ( !VideoSequence_LoadInternal( &stream->sequences[sequenceID], streamReader, allocator, stack ) )
			return false;

		stream->frameOffsets[sequenceID] = frameOffset;
		frameOffset += stream->sequences[sequenceID].desc.frameCount;
	}

	stream->frameOffsets[stream->desc.sequenceCount] = frameOffset;
	
	return true;
}

static void VideoFrame_DeferredLoad_Job( const void* args )
{
	V6_CPU_EVENT_SCOPE( s_cpuEventDecode );

	const VideoFrameJob_s* job = (const VideoFrameJob_s*)args;

	CBufferReader bufferReader( job->frameData, ToX64( job->frameDataSize ) );

	VideoSequence_s* sequence = &job->prefetcher->stream->sequences[job->sequenceID];
	void* buffer;
	const u32 frameSize = Codec_ReadFrame( &buffer, &bufferReader, &sequence->frameDescArray[job->frameRank], &sequence->frameDataArray[job->frameRank], job->frameRank, job->heap, job->stack );
	sequence->frameBufferArray[job->frameRank] = buffer;

	VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &job->prefetcher->sequencePrefetchInfos[job->sequenceID];
	const u32 pendingFrameCount = Atomic_Dec( &sequencePrefetchInfo->pendingFrameCount );
	Atomic_Add( &sequencePrefetchInfo->allocatedSize, frameSize );
	V6_ASSERT( pendingFrameCount > 0 );
}

static void VideoStreamPrefetcher_FreeSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID )
{
	VideoSequence_s* sequence = &prefetcher->stream->sequences[sequenceID];
	VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];

	V6_ASSERT( !sequencePrefetchInfo->inUse );
	V6_ASSERT( sequencePrefetchInfo->pendingFrameCount != VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT );

	VideoSequence_Release( sequence, &prefetcher->queueAllocator );

	prefetcher->queueAllocator.pop();

	sequencePrefetchInfo->filemapRegionID = Filemap_s::INVALID_REGION;
	sequencePrefetchInfo->pendingFrameCount = VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT;
}

static VideoStreamGetSequenceStatus_e VideoStreamPrefetcher_DeferredLoadSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID )
{
	VideoSequence_s* sequence = &prefetcher->stream->sequences[sequenceID];
	VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];

	const u32 sequenceRegionID = sequencePrefetchInfo->filemapRegionID;
	V6_ASSERT( sequenceRegionID != Filemap_s::INVALID_REGION );

	const u32 sequencePendingFrameCount = sequencePrefetchInfo->pendingFrameCount;
	if ( sequencePendingFrameCount == VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT )
	{
		u8* sequenceData = nullptr;
		{
			CODEC_SCOPED_HITCH_DETECTION( Filemap_GetRegionData, 100 );
			sequenceData = (u8*)Filemap_GetRegionData( &prefetcher->filemap, sequenceRegionID );
			if ( !sequenceData )
				return VIDEO_STREAM_GET_SEQUENCE_LOADING;
		}
		
		sequencePrefetchInfo->dispatchTime = Tick_GetCount();

		CBufferReader bufferReader( sequenceData, ToX64( (sequencePrefetchInfo+1)->streamOffset - (sequencePrefetchInfo+0)->streamOffset ) );

		if ( !Codec_ReadSequenceDesc( &bufferReader, &sequence->desc ) )
			return VIDEO_STREAM_GET_SEQUENCE_FAILED;

		prefetcher->queueAllocator.push();
		sequencePrefetchInfo->pendingFrameCount = 0;
		sequencePrefetchInfo->allocatedSize = 0;

		if ( sequence->desc.frameCount == 0 )
			return VIDEO_STREAM_GET_SEQUENCE_SUCCEEDED;

		const u8* frameData = sequenceData + ToU64( bufferReader.GetPos() );

		{
			CODEC_SCOPED_HITCH_DETECTION( Dispatch, 100 );

			u32 workerThreadLoads[VideoStreamPrefetcher_s::WORKER_THREAD_MAX_COUNT] = {};

			for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
			{
				const u32 frameDataSize = Codec_ReadFrameSizeOnly( &bufferReader );
				if ( frameDataSize == 0 )
					return VIDEO_STREAM_GET_SEQUENCE_FAILED;

				u32 availableWorkerThreadID = 0;
				u32 minLoad = workerThreadLoads[0];
				for ( u32 workerThreadID = 1; workerThreadID < VideoStreamPrefetcher_s::WORKER_THREAD_MAX_COUNT; ++workerThreadID )
				{
					const u32 load = workerThreadLoads[workerThreadID];
					if ( load < minLoad )
					{
						availableWorkerThreadID = workerThreadID;
						minLoad = load;
					}
				}

				{
					VideoFrameJob_s job = {};
					job.heap = &prefetcher->queueAllocator;
					job.stack = &prefetcher->workerStacks[availableWorkerThreadID];
					job.prefetcher = prefetcher;
					job.frameData = frameData;
					job.frameDataSize = frameDataSize;
					job.sequenceID = sequenceID;
					job.frameRank = frameRank;

					Atomic_Inc( &sequencePrefetchInfo->pendingFrameCount );
					WorkerThread_AddJob( &prefetcher->workerThreads[availableWorkerThreadID], VideoFrame_DeferredLoad_Job, &job, sizeof( job ) );
				}

				workerThreadLoads[availableWorkerThreadID] += frameDataSize;

				frameData += frameDataSize;
			}
		}

		return VIDEO_STREAM_GET_SEQUENCE_LOADING;
	}

	if ( sequencePendingFrameCount > 0 )
		return VIDEO_STREAM_GET_SEQUENCE_LOADING;

	Filemap_UnlockRegion( &prefetcher->filemap, sequenceRegionID );

	for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
	{
		void* frameBuffer = sequence->frameBufferArray[frameRank];
		if ( !frameBuffer )
		{
			VideoStreamPrefetcher_FreeSequence( prefetcher, sequenceID );
			return VIDEO_STREAM_GET_SEQUENCE_FAILED;
		}

		if ( (sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION) != 0 )
			sequence->frameBufferArray[frameRank] = nullptr;
	}

	const u32 allocatedSequenceCount = (u32)(prefetcher->allocatedSequenceQueueEnd - prefetcher->allocatedSequenceQueueBegin);
	if ( allocatedSequenceCount == VideoStreamPrefetcher_s::ALLOCATED_SEQUENCE_MAX_COUNT )
	{
		const u32 queuePos = prefetcher->allocatedSequenceQueueBegin & VideoStreamPrefetcher_s::ALLOCATED_SEQUENCE_MAX_MOD;
		const u32 allocatedSequenceID = prefetcher->allocatedSequenceQueue[queuePos];
		if ( Codec_Error() || prefetcher->sequencePrefetchInfos[allocatedSequenceID].inUse )
		{
			V6_ERROR( "Too many sequences in use\n" );
			return VIDEO_STREAM_GET_SEQUENCE_FAILED;
		}
		prefetcher->allocatedSequenceSize -= prefetcher->sequencePrefetchInfos[allocatedSequenceID].allocatedSize;
		VideoStreamPrefetcher_FreeSequence( prefetcher, allocatedSequenceID );
		++prefetcher->allocatedSequenceQueueBegin;
	}

	const u32 queuePos = prefetcher->allocatedSequenceQueueEnd & VideoStreamPrefetcher_s::ALLOCATED_SEQUENCE_MAX_MOD;
	prefetcher->allocatedSequenceQueue[queuePos] = sequenceID;
	++prefetcher->allocatedSequenceQueueEnd;
	prefetcher->allocatedSequenceSize += sequencePrefetchInfo->allocatedSize;

	sequencePrefetchInfo->unlockTime = Tick_GetCount();

	return VIDEO_STREAM_GET_SEQUENCE_SUCCEEDED;
}

static bool VideoStreamPrefetcher_PrefetchSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID )
{
	const u32 prefetchedSequenceCount = (u32)(prefetcher->prefetchedSequenceQueueEnd - prefetcher->prefetchedSequenceQueueBegin);
	if ( prefetchedSequenceCount == VideoStreamPrefetcher_s::PREFECTHED_SEQUENCE_MAX_COUNT )
		return false;

	VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];

	if ( sequencePrefetchInfo->pendingFrameCount == 0 )
		return true;

	if ( sequencePrefetchInfo->filemapRegionID != Filemap_s::INVALID_REGION )
		return true;

	sequencePrefetchInfo->lockTime = Tick_GetCount();

	VideoSequence_s* sequence = &prefetcher->stream->sequences[sequenceID];
	memset( sequence->frameBufferArray, 0, sequence->desc.frameCount * sizeof( void* ) );

	const u32 sequenceSize = (u32)((sequencePrefetchInfo+1)->streamOffset - (sequencePrefetchInfo+0)->streamOffset);
	sequencePrefetchInfo->filemapRegionID = Filemap_LockRegion( &prefetcher->filemap, sequencePrefetchInfo->streamOffset, sequenceSize );
	V6_ASSERT( sequencePrefetchInfo->filemapRegionID != Filemap_s::INVALID_REGION );

	const u32 queuePos = prefetcher->prefetchedSequenceQueueEnd & VideoStreamPrefetcher_s::PREFECTHED_SEQUENCE_MAX_MOD;
	prefetcher->prefetchedSequenceQueue[queuePos] = sequenceID;
	++prefetcher->prefetchedSequenceQueueEnd;

	return true;
}

static void VideoStreamPrefetcher_InitSequences( VideoStreamPrefetcher_s* prefetcher, VideoStream_s* stream, IStreamReader* streamReader, IAllocator* allocator )
{
	prefetcher->stream = stream;

	Filemap_SetStreamReader( &prefetcher->filemap, streamReader );

	prefetcher->sequencePrefetchInfos = allocator->newArray< VideoSequencePrefetchInfo_s >( stream->desc.sequenceCount+1, "VideoStreamPrefetchInfos" );
	memset( prefetcher->sequencePrefetchInfos, 0, stream->desc.sequenceCount * sizeof( *prefetcher->sequencePrefetchInfos ) );

	u64 streamOffset = ToU64( streamReader->GetPos() );
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		const CodecSequenceInfo_s* sequenceInfo = &stream->data.sequenceInfos[sequenceID];
		const u32 sequenceSize = (sequenceInfo->fadeToBlack1_frameCount7_size24 & 0xFFFFFF) << 4;

		VideoSequencePrefetchInfo_s* prefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];
		prefetchInfo->streamOffset = streamOffset;
		prefetchInfo->filemapRegionID = Filemap_s::INVALID_REGION;
		prefetchInfo->pendingFrameCount = VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT;

		streamOffset += sequenceSize;
	}

	VideoSequencePrefetchInfo_s* prefetchInfo = &prefetcher->sequencePrefetchInfos[stream->desc.sequenceCount];
	prefetchInfo->streamOffset = streamOffset;
	prefetchInfo->filemapRegionID = Filemap_s::INVALID_REGION;
	prefetchInfo->pendingFrameCount = VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT;
}

bool VideoSequence_Load( VideoSequence_s* sequence, IStreamReader* streamReader, IAllocator* allocator, IStack* stack )
{
	memset( sequence, 0, sizeof( *sequence ) );

	if ( !VideoSequence_LoadInternal( sequence, streamReader, allocator, stack ) )
	{
		VideoSequence_Release( sequence, allocator );
		return false;
	}

	return true;
}

void VideoSequence_Release( VideoSequence_s* sequence, IAllocator* allocator )
{
	for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
		allocator->free( sequence->frameBufferArray[frameRank] );
	memset( sequence, 0, sizeof( VideoSequence_s ) );
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


			DecoderBlock_s* rawBlocks = stack.newArray< DecoderBlock_s >( blockCount, "VideoStreamRawBlock" );

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

			DecoderBlock_s* sequenceBlocks = stack.newArray< DecoderBlock_s >( blockCount, "VideoStreamSequenceBlock" );

			// Load sequence blocks
			{
				for ( u32 rangeID = 0; rangeID < sequence->frameDescArray[frameRank].blockRangeCount; ++rangeID )
				{
					const CodecBlockRange_s* blockRange = &sequence->frameDataArray[frameRank].blockRanges[rangeID];
					const u32 rangeFrameRank = blockRange->frameRank7_newBlock1_firstBlockID24 >> 25;
					const u32 firstBlockID = blockRange->frameRank7_newBlock1_firstBlockID24 & 0xFFFFFF;
					const u32 rangeBlockCount = blockRange->blockCount;

					for ( u32 blockRank = 0; blockRank < rangeBlockCount; ++blockRank )
					{
						const u32 blockID = firstBlockID + blockRank;
						const u32 sequencePackedBlockPos = sequence->frameDataArray[rangeFrameRank].blockPos[blockID];
						DecoderBlock_s* sequenceBlock = &sequenceBlocks[blockID];

						if ( stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
						{
							const u32 mip = sequencePackedBlockPos >> 28;
							const u32 x = ((sequencePackedBlockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 0)) & CODEC_MIP_MACRO_XYZ_BIT_MASK);
							const u32 y = ((sequencePackedBlockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 1)) & CODEC_MIP_MACRO_XYZ_BIT_MASK);
							const u32 z = ((sequencePackedBlockPos >> (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 2)) & CODEC_MIP_MACRO_XYZ_BIT_MASK);

							Vec3i gridOffset = Vec3i_Zero();
							if ( stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
							{
								gridOffset =
									Codec_ComputeMacroGridCoords( &sequence->frameDescArray[rangeFrameRank].gridOrigin, gridScales[mip], gridMacroHalfWidth ) -
									Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameRank].gridOrigin, gridScales[mip], gridMacroHalfWidth );
							}

							sequenceBlock->packedBlockPos = mip << 28;
							sequenceBlock->packedBlockPos |= (x + gridOffset.x) << (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 0);
							sequenceBlock->packedBlockPos |= (y + gridOffset.y) << (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 1);
							sequenceBlock->packedBlockPos |= (z + gridOffset.z) << (CODEC_MIP_MACRO_XYZ_BIT_COUNT * 2);
						}
						else
						{
							sequenceBlock->packedBlockPos = sequencePackedBlockPos;
						}
							
						EncodedBlockEx_s encodedBlock;
						encodedBlock.cellEndColors = sequence->frameDataArray[rangeFrameRank].blockCellEndColors[blockID];
						encodedBlock.cellPresence = sequence->frameDataArray[rangeFrameRank].blockCellPresences0[blockID] | ((u64)sequence->frameDataArray[rangeFrameRank].blockCellPresences1[blockID] << 32);
						encodedBlock.cellColorIndices[0] = sequence->frameDataArray[rangeFrameRank].blockCellColorIndices0[blockID] | ((u64)sequence->frameDataArray[rangeFrameRank].blockCellColorIndices1[blockID] << 32);
						encodedBlock.cellColorIndices[1] = sequence->frameDataArray[rangeFrameRank].blockCellColorIndices2[blockID] | ((u64)sequence->frameDataArray[rangeFrameRank].blockCellColorIndices3[blockID] << 32);

						Block_Decode( sequenceBlock->cellRGBA, &sequenceBlock->cellCount, &encodedBlock );
					}
				}
					
				u32* rawFrameBlockIDs = stack.newArray< u32 >( blockCount, "VideoStreamRawFrameBlockID" );
				for ( u32 blockRank = 0; blockRank < blockCount; ++blockRank )
					rawFrameBlockIDs[blockRank] = blockRank;

				qsort_s( rawFrameBlockIDs, blockCount, sizeof( u32 ), Block_ComparePos, rawBlocks );

				u32* sequenceBlockIDs = stack.newArray< u32 >( blockCount, "VideoStreamSequenceBlockID" );
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
	if ( !fileReader.Open( streamFilename, FILE_OPEN_FLAG_UNBUFFERED ) )
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
		return nullptr;
	}

	return Codec_ReadStreamDesc( &fileReader, streamDesc, nullptr, nullptr ) != nullptr;
}

void* VideoStream_LoadDescAndData( const char* streamFilename, CodecStreamDesc_s* streamDesc, CodecStreamData_s* streamData, IAllocator* allocator )
{	
	CFileReader fileReader;
	if ( !fileReader.Open( streamFilename, 0 ) )
	{
		V6_ERROR( "Unable to open file %s\n", streamFilename );
		return nullptr;
	}

	return Codec_ReadStreamDesc( &fileReader, streamDesc, streamData, allocator );
}

u8* VideoStream_GetKeyValue( u32* valueSize, const CodecStreamDesc_s* streamDesc, const CodecStreamData_s* streamData, const char* key, IAllocator* allocator )
{
	u32 keyOffet = 0;
	u32 valueOffet = 0;
	for ( u32 keyID = 0; keyID < streamDesc->keyCount; ++keyID )
	{
		if ( _stricmp( key, streamData->keys + keyOffet ) == 0 )
		{
			if ( streamDesc->valueSizes[keyID] == 0 )
				return nullptr;

			u8* value = (u8*)allocator->alloc( streamDesc->valueSizes[keyID], "VideoStreamValue" );
			memcpy( value, streamData->values + valueOffet, streamDesc->valueSizes[keyID] );
			*valueSize = streamDesc->valueSizes[keyID];
			return value;
		}
		keyOffet += streamDesc->keySizes[keyID];
		valueOffet += streamDesc->valueSizes[keyID];
	}

	// V6_MSG( "Key %s not found.\n", key );
	return nullptr;
}

void VideoStream_Release( VideoStream_s* stream, IAllocator* allocator )
{
	if ( stream->buffer != nullptr )
	{
		if ( stream->sequences )
		{
			for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
				VideoSequence_Release( &stream->sequences[sequenceID], allocator );
			allocator->deleteArray( stream->sequences );
		}
		allocator->deleteArray( stream->frameOffsets );
		allocator->free( stream->buffer );
	}

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
		if ( frameID < stream->frameOffsets[midSequenceID] )
			maxSequenceID = midSequenceID;
		else
			minSequenceID = midSequenceID;
	}
	
	return minSequenceID;
}

bool VideoStreamPrefetcher_Process( VideoStreamPrefetcher_s* prefetcher )
{
	CODEC_SCOPED_HITCH_DETECTION( Process, 100 );

	const u32 prefetchedSequenceCount = (u32)(prefetcher->prefetchedSequenceQueueEnd - prefetcher->prefetchedSequenceQueueBegin);
	u32 prefetchedSequenceRankUndone;
	for ( prefetchedSequenceRankUndone = 0; prefetchedSequenceRankUndone < prefetchedSequenceCount; ++prefetchedSequenceRankUndone )
	{
		const u32 queuePos = (prefetcher->prefetchedSequenceQueueBegin + prefetchedSequenceRankUndone) & VideoStreamPrefetcher_s::PREFECTHED_SEQUENCE_MAX_MOD;
		const u32 prefetchedSequenceID = prefetcher->prefetchedSequenceQueue[queuePos];
		VideoStreamGetSequenceStatus_e deferredStatus = VideoStreamPrefetcher_DeferredLoadSequence( prefetcher, prefetchedSequenceID );
		if ( deferredStatus == VIDEO_STREAM_GET_SEQUENCE_FAILED )
			return false;
		if ( deferredStatus == VIDEO_STREAM_GET_SEQUENCE_LOADING )
			break;
	}
	prefetcher->prefetchedSequenceQueueBegin += prefetchedSequenceRankUndone;

	while ( prefetcher->allocatedSequenceSize > VideoStreamPrefetcher_s::SEQUENCE_CACHE_CAPACITY )
	{
		V6_ASSERT( prefetcher->allocatedSequenceQueueBegin < prefetcher->allocatedSequenceQueueEnd );
		const u32 queuePos = prefetcher->allocatedSequenceQueueBegin & VideoStreamPrefetcher_s::ALLOCATED_SEQUENCE_MAX_MOD;
		const u32 allocatedSequenceID = prefetcher->allocatedSequenceQueue[queuePos];
		VideoSequencePrefetchInfo_s* allocatedSequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[allocatedSequenceID];
		if ( allocatedSequencePrefetchInfo->inUse )
			break;

		CODEC_SCOPED_HITCH_DETECTION( FreeSequence, 100 );

		V6_ASSERT( allocatedSequencePrefetchInfo->allocatedSize <= prefetcher->allocatedSequenceSize );
		prefetcher->allocatedSequenceSize -= allocatedSequencePrefetchInfo->allocatedSize;
		VideoStreamPrefetcher_FreeSequence( prefetcher, allocatedSequenceID );
		++prefetcher->allocatedSequenceQueueBegin;
	}

	return true;
}

VideoStreamGetSequenceStatus_e VideoStreamPrefetcher_GetSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID, bool fillBuffer )
{
	CODEC_SCOPED_HITCH_DETECTION( VideoStream_TryToGetSequence_Inner, 100 );

	const u32 prefetchedFrameTargetCount = (u32)(prefetcher->prefetchDuration * prefetcher->stream->desc.frameRate);

	VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];
	V6_ASSERT( !sequencePrefetchInfo->inUse );

	// prefetch sequences

	{
		CODEC_SCOPED_HITCH_DETECTION( Prefetch, 100 );

		u32 prefetchedFrameCount = 0;
		const u32 frameOffset = prefetcher->stream->frameOffsets[sequenceID];
		for ( u32 prefetchSequenceID = sequenceID; prefetchSequenceID < prefetcher->stream->desc.sequenceCount && prefetchedFrameCount < prefetchedFrameTargetCount; ++prefetchSequenceID )
		{
			if ( !VideoStreamPrefetcher_PrefetchSequence( prefetcher, prefetchSequenceID ) )
				break;
			prefetchedFrameCount = prefetcher->stream->frameOffsets[prefetchSequenceID+1] - frameOffset; 
		}
	}

	const VideoStreamGetSequenceStatus_e status = prefetcher->sequencePrefetchInfos[sequenceID].pendingFrameCount > 0 ? VIDEO_STREAM_GET_SEQUENCE_LOADING : VIDEO_STREAM_GET_SEQUENCE_SUCCEEDED;

	if ( !VideoStreamPrefetcher_Process( prefetcher ) )
		return VIDEO_STREAM_GET_SEQUENCE_FAILED;

	if ( fillBuffer && prefetcher->prefetchedSequenceQueueBegin < prefetcher->prefetchedSequenceQueueEnd )
		return VIDEO_STREAM_GET_SEQUENCE_LOADING;

	if ( status == VIDEO_STREAM_GET_SEQUENCE_SUCCEEDED )
		sequencePrefetchInfo->inUse = true;

	return status;
}

void VideoStreamPrefetcher_ReleaseSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID )
{
	VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];
	V6_ASSERT( sequencePrefetchInfo->inUse );

	sequencePrefetchInfo->inUse = false;
}

void VideoStreamPrefetcher_Create( VideoStreamPrefetcher_s* prefetcher, float preftechDuration, IAllocator* heap )
{
	const u32 cacheCapacity = VideoStreamPrefetcher_s::FILEMAP_CACHE_CAPACITY;
	prefetcher->cache = (u8*)heap->alloc( cacheCapacity, "VideoStreamCache" );

	Filemap_Create( &prefetcher->filemap, prefetcher->cache, cacheCapacity );

	for ( u32 workerThreadID = 0; workerThreadID < VideoStreamPrefetcher_s::WORKER_THREAD_MAX_COUNT; ++workerThreadID )
	{
		WorkerThread_Create( &prefetcher->workerThreads[workerThreadID], THREAD_ANY_CORE );
		prefetcher->workerStacks[workerThreadID].Init( heap, MulMB( 64 ) );
	}

	prefetcher->queueAllocator.Init( heap, VideoStreamPrefetcher_s::QUEUE_ALLOCATOR_CAPACITY );

	prefetcher->allocatedSequenceQueueBegin = 0;
	prefetcher->allocatedSequenceQueueEnd = 0;
	prefetcher->allocatedSequenceSize = 0;

	prefetcher->prefetchedSequenceQueueBegin = 0;
	prefetcher->prefetchedSequenceQueueEnd = 0;

	prefetcher->sequencePrefetchInfos = nullptr;

	prefetcher->prefetchDuration = preftechDuration;

	V6_MSG( "Stream prefetcher created\n" );
}

void VideoStreamPrefetcher_Release( VideoStreamPrefetcher_s* prefetcher, IAllocator* heap )
{
	V6_ASSERT( prefetcher->sequencePrefetchInfos == nullptr );

	Filemap_WaitForIdle( &prefetcher->filemap );
	heap->free( prefetcher->cache );
	Filemap_Release( &prefetcher->filemap );

	for ( u32 workerThreadID = 0; workerThreadID < VideoStreamPrefetcher_s::WORKER_THREAD_MAX_COUNT; ++workerThreadID )
	{
		WorkerThread_Release( &prefetcher->workerThreads[workerThreadID] );
		prefetcher->workerStacks[workerThreadID].Release();
	}
	
	prefetcher->queueAllocator.Release();
}

void VideoStreamPrefetcher_CancelAllPendingSequences( VideoStreamPrefetcher_s* prefetcher )
{
	Filemap_CancelAllRegions( &prefetcher->filemap );

	for ( u32 workerThreadID = 0; workerThreadID < VideoStreamPrefetcher_s::WORKER_THREAD_MAX_COUNT; ++workerThreadID )
		WorkerThread_CancelAllJobs( &prefetcher->workerThreads[workerThreadID] );
}

void VideoStreamPrefetcher_ReleaseSequences( VideoStreamPrefetcher_s* prefetcher, IAllocator* allocator )
{
	V6_ASSERT( Filemap_GetPendingRegionCount( &prefetcher->filemap ) == 0 );

	for ( u32 workerThreadID = 0; workerThreadID < VideoStreamPrefetcher_s::WORKER_THREAD_MAX_COUNT; ++workerThreadID )
	{
		V6_ASSERT( WorkerThread_GetPendingJobCount( &prefetcher->workerThreads[workerThreadID] ) == 0 );
	}

	while ( prefetcher->allocatedSequenceQueueBegin < prefetcher->allocatedSequenceQueueEnd )
	{
		const u32 queuePos = prefetcher->allocatedSequenceQueueBegin & VideoStreamPrefetcher_s::ALLOCATED_SEQUENCE_MAX_MOD;
		const u32 allocatedSequenceID = prefetcher->allocatedSequenceQueue[queuePos];
		VideoSequencePrefetchInfo_s* allocatedSequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[allocatedSequenceID];
		V6_ASSERT( !allocatedSequencePrefetchInfo->inUse );
		V6_ASSERT( allocatedSequencePrefetchInfo->allocatedSize <= prefetcher->allocatedSequenceSize );
		prefetcher->allocatedSequenceSize -= allocatedSequencePrefetchInfo->allocatedSize;
		VideoStreamPrefetcher_FreeSequence( prefetcher, allocatedSequenceID );
		++prefetcher->allocatedSequenceQueueBegin;
	}

	while ( prefetcher->prefetchedSequenceQueueBegin < prefetcher->prefetchedSequenceQueueEnd )
	{
		const u32 queuePos = prefetcher->allocatedSequenceQueueBegin & VideoStreamPrefetcher_s::ALLOCATED_SEQUENCE_MAX_MOD;
		const u32 prefetchedSequenceID = prefetcher->prefetchedSequenceQueue[queuePos];
		VideoSequencePrefetchInfo_s* prefetchedSequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[prefetchedSequenceID];
		if ( prefetchedSequencePrefetchInfo->pendingFrameCount != VIDEO_PREFETCH_SEQUENCE_INVALID_PENDING_FRAME_COUNT )
			VideoStreamPrefetcher_FreeSequence( prefetcher, prefetchedSequenceID );
		++prefetcher->prefetchedSequenceQueueBegin;
	}

#if 1
	for ( u32 sequenceID = 0; sequenceID < prefetcher->stream->desc.sequenceCount; ++sequenceID )
	{
		VideoSequence_s* sequence = &prefetcher->stream->sequences[sequenceID];
		for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
			V6_ASSERT( sequence->frameBufferArray[frameRank] == nullptr );
	}
#endif

	prefetcher->allocatedSequenceQueueBegin = 0;
	prefetcher->allocatedSequenceQueueEnd = 0;

	prefetcher->prefetchedSequenceQueueBegin = 0;
	prefetcher->prefetchedSequenceQueueEnd = 0;

	V6_ASSERT( prefetcher->allocatedSequenceSize == 0 );

	allocator->deleteArray(	prefetcher->sequencePrefetchInfos );
	prefetcher->sequencePrefetchInfos = nullptr;
}

bool VideoStream_InitWithPrefetcher( VideoStream_s* stream, VideoStreamPrefetcher_s* prefetcher, CFileReader* fileReader, const char* streamFilename, IAllocator* allocator )
{
	memset( stream, 0, sizeof( *stream ) );

	if ( Codec_Error() || !fileReader->Open( streamFilename, FILE_OPEN_FLAG_UNBUFFERED ) )
	{
		V6_ERROR( "Unable to open file %s\n", streamFilename );
		goto clean_up;
	}

	stream->buffer = Codec_ReadStreamDesc( fileReader, &stream->desc, &stream->data, allocator );
	if ( !stream->buffer )
		goto clean_up;

	if ( Codec_Error() || stream->desc.sequenceCount == 0 )
	{
		V6_ERROR( "No sequence in stream %s\n", streamFilename );
		goto clean_up;
	}

	if ( Codec_Error() || stream->desc.frameCount == 0 )
	{
		V6_ERROR( "No frame in stream %s\n", streamFilename );
		goto clean_up;
	}

	if ( Codec_Error() || stream->data.sequenceInfos == nullptr )
	{
		V6_ERROR( "Stream %s is not up-to-date\n", streamFilename );
		goto clean_up;
	}

	strcpy_s( stream->name, sizeof( stream->name ), streamFilename );

	stream->sequences = allocator->newArray< VideoSequence_s >( stream->desc.sequenceCount, "VideoStreamSequences" );
	memset( stream->sequences, 0, stream->desc.sequenceCount * sizeof( *stream->sequences ) );

	stream->frameOffsets = allocator->newArray< u32 >( stream->desc.sequenceCount+1, "VideoStreamFrameOffsets" );
	u32 frameOffset = 0;
	
	for ( u32 sequenceID = 0; sequenceID < stream->desc.sequenceCount; ++sequenceID )
	{
		const CodecSequenceInfo_s* sequenceInfo = &stream->data.sequenceInfos[sequenceID];
		const u32 frameCount = (sequenceInfo->fadeToBlack1_frameCount7_size24 >> 24) & 0x7F;
		stream->frameOffsets[sequenceID] = frameOffset;
		frameOffset += frameCount;
	}
	stream->frameOffsets[stream->desc.sequenceCount] = frameOffset;

	VideoStreamPrefetcher_InitSequences( prefetcher, stream, fileReader, allocator );

	return true;

clean_up:
	fileReader->Close();
	VideoStream_Release( stream, allocator );

	return false;
}

END_V6_NAMESPACE
