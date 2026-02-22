/*V6*/

#pragma once

#ifndef __V6_CODEC_DECODER_H__
#define __V6_CODEC_DECODER_H__

#include <v6/codec/codec.h>
#include <v6/core/filemap.h>
#include <v6/core/memory.h>

BEGIN_V6_NAMESPACE

class CFileReader;

struct VideoSequence_s
{
	CodecSequenceDesc_s			desc;
	CodecFrameDesc_s			frameDescArray[CODEC_FRAME_MAX_COUNT];
	CodecFrameData_s			frameDataArray[CODEC_FRAME_MAX_COUNT];
	void*						frameBufferArray[CODEC_FRAME_MAX_COUNT];
};

struct VideoSequencePrefetchInfo_s
{
	u64							streamOffset;
	u32							filemapRegionID;
	u32							pendingFrameCount;
	u32							allocatedSize;
	u64							lockTime;
	u64							dispatchTime;
	u64							unlockTime;
	b32							inUse;
};

struct VideoStream_s
{
	char						name[256];
	CodecStreamDesc_s			desc;
	CodecStreamData_s			data;
	void*						buffer;
	VideoSequence_s*			sequences;
	u32*						frameOffsets;
};

struct VideoStreamPrefetcher_s
{
	static const u32				FILEMAP_CACHE_CAPACITY			= MulMB_ConstExpr(  512 );
	static const u32				SEQUENCE_CACHE_CAPACITY			= MulMB_ConstExpr( 1512 );
	static const u32				QUEUE_ALLOCATOR_CAPACITY		= SEQUENCE_CACHE_CAPACITY + MulMB_ConstExpr( 512 );

	static const u32				WORKER_THREAD_MAX_COUNT			= 2;
	static const u32				ALLOCATED_SEQUENCE_MAX_COUNT	= 16;
	static const u32				ALLOCATED_SEQUENCE_MAX_MOD		= ALLOCATED_SEQUENCE_MAX_COUNT-1;
	static const u32				PREFECTHED_SEQUENCE_MAX_COUNT	= 16;
	static const u32				PREFECTHED_SEQUENCE_MAX_MOD		= PREFECTHED_SEQUENCE_MAX_COUNT-1;

	Filemap_s						filemap;
	WorkerThread_s					workerThreads[WORKER_THREAD_MAX_COUNT];
	Stack							workerStacks[WORKER_THREAD_MAX_COUNT];
	QueueAllocator					queueAllocator;
	VideoStream_s*					stream;
	u32								allocatedSequenceQueue[ALLOCATED_SEQUENCE_MAX_COUNT];
	u64								allocatedSequenceQueueBegin;
	u64								allocatedSequenceQueueEnd;
	u64								allocatedSequenceSize;
	u32								prefetchedSequenceQueue[PREFECTHED_SEQUENCE_MAX_COUNT];
	u64								prefetchedSequenceQueueBegin;
	u64								prefetchedSequenceQueueEnd;
	u8*								cache;
	VideoSequencePrefetchInfo_s*	sequencePrefetchInfos;
	float							prefetchDuration;
};
V6_STATIC_ASSERT( IsPowOfTwo_ConstExpr( VideoStreamPrefetcher_s::PREFECTHED_SEQUENCE_MAX_COUNT ) );

enum VideoStreamGetSequenceStatus_e : u32
{
	VIDEO_STREAM_GET_SEQUENCE_FAILED,
	VIDEO_STREAM_GET_SEQUENCE_LOADING,
	VIDEO_STREAM_GET_SEQUENCE_SUCCEEDED,
};

bool							VideoSequence_Load( VideoSequence_s* sequence, IStreamReader* streamReader, IAllocator* allocator, IStack* stack );
void							VideoSequence_Release( VideoSequence_s* sequence, IAllocator* allocator );

u32								VideoStream_FindSequenceIDFromFrameID( const VideoStream_s* stream, u32 frameID );
u8*								VideoStream_GetKeyValue( u32* valueSize, const CodecStreamDesc_s* streamDesc, const CodecStreamData_s* streamData, const char* key, IAllocator* allocator );
bool							VideoStream_InitWithPrefetcher( VideoStream_s* stream, VideoStreamPrefetcher_s* prefetcher, CFileReader* fileReader, const char* streamFilename, IAllocator* allocator );
bool							VideoStream_LoadDesc( const char* streamFilename, CodecStreamDesc_s* streamDesc );
void*							VideoStream_LoadDescAndData( const char* streamFilename, CodecStreamDesc_s* streamDesc, CodecStreamData_s* streamData, IAllocator* allocator );
bool							VideoStream_Load( VideoStream_s* stream, const char* streamFilename, IAllocator* allocator, IStack* stack );
void							VideoStream_Release( VideoStream_s* stream, IAllocator* allocator );
bool							VideoStream_Validate( const VideoStream_s* stream, const char* templateFilename, u32 frameOffset, IAllocator* allocator );

void							VideoStreamPrefetcher_CancelAllPendingSequences( VideoStreamPrefetcher_s* prefetcher );
void							VideoStreamPrefetcher_Create( VideoStreamPrefetcher_s* prefetcher, float preftechDuration, IAllocator* heap );
VideoStreamGetSequenceStatus_e	VideoStreamPrefetcher_GetSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID, bool fillBuffer );
bool							VideoStreamPrefetcher_Process( VideoStreamPrefetcher_s* prefetcher );
void							VideoStreamPrefetcher_Release( VideoStreamPrefetcher_s* prefetcher, IAllocator* heap );
void							VideoStreamPrefetcher_ReleaseSequence( VideoStreamPrefetcher_s* prefetcher, u32 sequenceID );
void							VideoStreamPrefetcher_ReleaseSequences( VideoStreamPrefetcher_s* prefetcher, IAllocator* allocator );

END_V6_NAMESPACE

#endif // __V6_CODEC_DECODER_H__
