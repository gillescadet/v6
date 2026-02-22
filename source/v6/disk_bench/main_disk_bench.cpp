/*V6*/

#include <v6/core/common.h>

#include <v6/codec/decoder.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/thread.h>
#include <v6/core/time.h>

BEGIN_V6_NAMESPACE

//----------------------------------------------------------------------------------------------------

void OutputMessage( u32 msgType, const char * format, ... )
{
	char buffer[4096];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof( buffer ), format, args);
	va_end( args );

#if 1
	fputs( buffer, stdout );
#else
	static FILE* f = nullptr;
	if ( f == nullptr )
		fopen_s( &f, "d:/tmp/log.txt", "wt" );
	fputs( buffer, f );
#endif
}

//----------------------------------------------------------------------------------------------------

bool VideoStream_FastLoad( VideoStreamPrefetcher_s* prefetcher, const char* streamFilename, float playSpeed, CHeap* heap, IStack* stack )
{
	const u64 startTime = Tick_GetCount();

	VideoStream_s stream;
	CFileReader fileReader;

	if ( !VideoStream_InitWithPrefetcher( &stream, prefetcher, &fileReader, streamFilename, heap ) )
		return false;

	bool success = false;

	bool fillBuffer = false;
	for ( u32 sequenceID = 0; sequenceID < stream.desc.sequenceCount; ++sequenceID )
	{
		const u64 sequenceStartTime = Tick_GetCount();

		bool isLoading = false;
		u64 waitingPacifier;
		for (;;)
		{
			VideoStreamGetSequenceStatus_e status;
			{
				status = VideoStreamPrefetcher_GetSequence( prefetcher, sequenceID, fillBuffer );
				fillBuffer = false;
			}

			if ( status == VIDEO_STREAM_GET_SEQUENCE_LOADING )
			{
				fillBuffer = true;
				if ( isLoading )
				{
					u64 time = Tick_GetCount();
					if ( Tick_ConvertToSeconds( time - waitingPacifier ) > 0.05f )
					{
						waitingPacifier = time;
						V6_MSG( "." );
					}
					Thread_Sleep( 1 );
				}
				else
				{
					V6_MSG( "Loading sequence %d", sequenceID );
					waitingPacifier = Tick_GetCount();
					isLoading = true;
				}
				continue;
			}

			if ( isLoading )
			{
				V6_MSG( "\n" );
				isLoading = false;
			}

			if ( status == VIDEO_STREAM_GET_SEQUENCE_FAILED )
			{
				V6_ERROR( "Failed to load sequence %d\n", sequenceID );
				goto clean_up;
			}

			const u64 sequenceEndTime = Tick_GetCount();
		
			V6_ASSERT( status == VIDEO_STREAM_GET_SEQUENCE_SUCCEEDED );
			
			VideoSequencePrefetchInfo_s* sequencePrefetchInfo = &prefetcher->sequencePrefetchInfos[sequenceID];
			V6_ASSERT( sequencePrefetchInfo->pendingFrameCount == 0 );
			const u32 sequenceSize = (u32)((sequencePrefetchInfo+1)->streamOffset - (sequencePrefetchInfo+0)->streamOffset);
			const float loadTime = Tick_ConvertToSeconds( sequencePrefetchInfo->dispatchTime - sequencePrefetchInfo->lockTime );
			const float decodeTime = Tick_ConvertToSeconds( sequencePrefetchInfo->unlockTime - sequencePrefetchInfo->dispatchTime );
			const float aheadTime = sequenceStartTime > sequencePrefetchInfo->unlockTime ? Tick_ConvertToSeconds( sequenceStartTime - sequencePrefetchInfo->unlockTime ) : -Tick_ConvertToSeconds( sequencePrefetchInfo->unlockTime - sequenceStartTime );
			const float waitTime = Tick_ConvertToSeconds( sequenceEndTime - sequenceStartTime );
			V6_MSG( "Loaded sequence %3d, load time: %6.1fms (%3d MB at %6.1f MB/s), decode time: %6.1fms, ahead time: %7.1fms, wait time: %6.1fms, size: %3d MB, heap: %d MB, queue: %d MB\n", 
				sequenceID,
				loadTime * 1000.0f, 
				DivMB( sequenceSize ),
                ((float)sequenceSize / (1024 * 1024)) / loadTime,
				decodeTime * 1000.0f,
				aheadTime * 1000.0f,
				waitTime * 1000.0f,
				DivMB( sequencePrefetchInfo->allocatedSize ),
				DivMB( heap->GetUsedSize() ),
				DivMB( prefetcher->queueAllocator.GetUsedSize() ));

			// Simulate frame processing
			float remainingTime = 1000.0f;
			for ( u32 frameID = 0; frameID < stream.sequences[sequenceID].desc.frameCount; ++frameID )
			{
				if ( !VideoStreamPrefetcher_Process( prefetcher ) )
				{
					VideoStreamPrefetcher_ReleaseSequence( prefetcher, sequenceID );
					goto clean_up;
				}

				const float deltaTime = Floor( remainingTime / (stream.sequences[sequenceID].desc.frameCount - frameID) );
				remainingTime -= deltaTime;
				Thread_Sleep( (u32)deltaTime );
			}

			VideoStreamPrefetcher_ReleaseSequence( prefetcher, sequenceID );

			break;
		}
	}

	const u64 stopTime = Tick_GetCount();

	const float time = Tick_ConvertToSeconds( stopTime - startTime );
	V6_MSG( "Read in %.2fms\n", time * 1000.0f );

	success = true;

clean_up:
	if ( !success )
		VideoStreamPrefetcher_CancelAllPendingSequences( prefetcher );
	VideoStreamPrefetcher_ReleaseSequences( prefetcher, heap );
	VideoStream_Release( &stream, heap );

	return success;
}

//----------------------------------------------------------------------------------------------------

bool ReadFile( const char* str, IStack* stack )
{
	ScopedStack scopedStack( stack );

	const u64 startTime = Tick_GetCount();

	CFileReader fileReader;
	if ( !fileReader.Open( str, FILE_OPEN_FLAG_UNBUFFERED ) )
	{
		V6_ERROR( "Unable to read %s\n", str );
		return false;
	}

	u64 bufferSize = MulMB( 8 );
	u8* buffer = (u8*)stack->alloc( bufferSize, "ReadFile" );
	u64 size = 0;


	u64 loopTimeBegin = Tick_GetCount();
	u64 loopSize = 0;

	for (;;)
	{
		u64 byteRead = ToU64( fileReader.Read( ToX64( bufferSize ), buffer ) );
		size += byteRead;
		if ( byteRead < bufferSize )
			break;

		u64 loopTimeEnd = Tick_GetCount();
		loopSize += byteRead;

		const float time = Tick_ConvertToSeconds( loopTimeEnd - loopTimeBegin );
		if ( time >= 1.0f )
		{
			V6_MSG( "Bandwidth: %.1f MB/s\n", ((float)loopSize / (1024 * 1024)) / time );

			loopTimeBegin = loopTimeEnd;
			loopSize = 0;
		}
	}

	const u64 stopTime = Tick_GetCount();

	const float time = Tick_ConvertToSeconds( stopTime - startTime );
	V6_MSG( "Read %lld bytes in %.2fms: %.1f MB/s\n", size, time * 1000.0f, ((float)size / (1024 * 1024)) / time );

	return true;
}

END_V6_NAMESPACE

int main( int argc, const char** argv )
{
	V6_MSG( "Disk Bench 0.0\n\n" );

	v6::CHeap heap;
	v6::Stack stack( &heap, v6::MulMB( 100 ) );

#if 0
	ReadFile( "D:/tmp/v6/Failure_Full_Cache.df", &stack );
#else
	v6::VideoStreamPrefetcher_s prefetcher;
	VideoStreamPrefetcher_Create( &prefetcher, 4.0f, &heap );

	VideoStream_FastLoad( &prefetcher, argv[1], 1.0f, &heap, &stack );

	VideoStreamPrefetcher_Release( &prefetcher, &heap );
#endif

	V6_MSG( "Done.\n" );
}
