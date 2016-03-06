/*V6*/

#pragma comment(lib, "core.lib")

#include <v6/core/common.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/thread.h>

using namespace v6;

typedef core::u32 uint;

#define GRID_CELL_BUCKET				0

#define HLSL_GRID_MACRO_SHIFT			5
#define HLSL_GRID_MACRO_2XSHIFT			(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT			(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_WIDTH			(1 << HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_MASK			(HLSL_GRID_MACRO_WIDTH-1)

#define HLSL_STREAM_THREAD_GROUP_SHIFT	4
#define HLSL_STREAM_THREAD_GROUP_SIZE	(1 << HLSL_STREAM_THREAD_GROUP_SHIFT)

#define HLSL_STREAM_SHIFT				((HLSL_GRID_MACRO_3XSHIFT + 4) - 5)
#define HLSL_STREAM_SIZE				(1 << HLSL_STREAM_SHIFT)
#define HLSL_STREAM_GROUP_SIZE			((HLSL_STREAM_SIZE >> HLSL_STREAM_THREAD_GROUP_SHIFT)>>5)
#define HLSL_STREAM_LAYER_COUNT			((HLSL_STREAM_SHIFT + HLSL_STREAM_THREAD_GROUP_SHIFT - 1) / HLSL_STREAM_THREAD_GROUP_SHIFT)

#define GROUP_COUNT( C, S )				(((C) + (S) - 1)) / (S)

#define block_count( BUCKET )			1000
#define block_posOffset( BUCKET )		0

#define groupshared

uint blockPositions[block_count( 0 )] = {};
uint sortedBlockPositions[block_count( 0 )] = {};
uint streamGroupBits[HLSL_STREAM_GROUP_SIZE] = {};
uint streamBits[HLSL_STREAM_SIZE] = {};
uint streamCounts[HLSL_STREAM_SIZE * 2] = {};
uint streamAddresses[HLSL_STREAM_SIZE] = {};

uint c_streamCurrentOffset = 0;
uint c_streamLowerOffset = 0;

static uint				s_threadDoneCount = 0;
static uint				s_barrier = 0;
static uint				s_concurrentBarrierCounts[2];
static core::Signal_s	s_threadGroupDone;
static core::Signal_s	s_threadGroupBarriers[2];

struct ThreadContext_s
{
	void (*kernel)( uint groupID, uint threadGroupID, uint threadID );
	uint groupID;
	uint threadGroupID;
	uint threadID;
};

uint countbits( uint n)
{	
	register unsigned int tmp;
	tmp = n - ((n >> 1) & 033333333333) - ((n >> 2) & 011111111111);
	return ((tmp + (tmp >> 3)) & 030707070707) % 63;
}

uint firstbithigh( uint value )
{
	unsigned long index;
	return _BitScanReverse( &index, value ) == 0 ? -1 : index;
}

void InterlockedOr( uint& value, uint mask )
{
	value |= mask;
}

void AllMemoryBarrierWithGroupSync()
{
	const uint barrier = s_barrier;
	if ( core::Atomic_Inc( &s_concurrentBarrierCounts[barrier] ) + 1 < HLSL_STREAM_THREAD_GROUP_SIZE )
	{
		core::Signal_Wait( &s_threadGroupBarriers[barrier] );
		V6_ASSERT( s_concurrentBarrierCounts[barrier] == HLSL_STREAM_THREAD_GROUP_SIZE );
	}
	else
	{
		s_barrier = 1 - barrier;
		core::Signal_Reset( &s_threadGroupBarriers[s_barrier] );
		s_concurrentBarrierCounts[s_barrier] = 0;

		core::Signal_Emit( &s_threadGroupBarriers[barrier] );
	}
}

void ScatterBlock( uint packedBlockPos, uint prevBlockID, uint newBlockID )
{
	sortedBlockPositions[newBlockID] = packedBlockPos;
}

#define EXPORT_STREAM_SET_BIT		1
#define EXPORT_STREAM_PREFIX_SUM	1
#define EXPORT_STREAM_SUMMARIZE		1
#define EXPORT_STREAM_SCATTER		1
#include <v6/viewer/stream_scan_cs_impl.hlsli>

static int CompareBlockPositionByKey( const void* p0, const void* p1 )
{
	const __int64 s0 = ComputeKeyFromPackedBlockPos( *((uint*)p0) );
	const __int64 s1 = ComputeKeyFromPackedBlockPos( *((uint*)p1) );
	if ( s0 == s1 )
		return 0;
	return s0 < s1 ? -1 : 1;
}

template < uint GROUP_SIZE >
void KernelWrapper( ThreadContext_s* threadContext )
{
	threadContext->kernel( threadContext->groupID, threadContext->threadGroupID, threadContext->threadID );
	
	if ( core::Atomic_Inc( &s_threadDoneCount ) + 1 == GROUP_SIZE )
		core::Signal_Emit( &s_threadGroupDone );
}

template < uint GROUP_SIZE >
void Dispatch( uint elementCount, void (*kernel)( uint groupID, uint threadGroupID, uint threadID ), const char* name )
{
	const uint groupCount = GROUP_COUNT( elementCount, GROUP_SIZE );

	printf( "\n" );

	uint threadID = 0;
	for ( uint groupID = 0; groupID < groupCount; ++groupID )
	{
		s_threadDoneCount = 0;
		core::Signal_Reset( &s_threadGroupDone );
		
		s_barrier = 0;
		core::Signal_Reset( &s_threadGroupBarriers[0] );
		s_concurrentBarrierCounts[0] = 0;

		ThreadContext_s threadContexts[GROUP_SIZE];
		for ( uint threadGroupID = 0; threadGroupID < GROUP_SIZE; ++threadGroupID, ++threadID )
		{
			threadContexts[threadGroupID].kernel = kernel;
			threadContexts[threadGroupID].groupID = groupID;
			threadContexts[threadGroupID].threadGroupID = threadGroupID;
			threadContexts[threadGroupID].threadID = threadID;
			core::Job_Launch( KernelWrapper< GROUP_SIZE >, &threadContexts[threadGroupID] );
		}

		core::Signal_Wait( &s_threadGroupDone );

		printf( "\rDispatch %s(%d/%d)", name, threadID, elementCount );
	}

	printf( "\n" );
}

bool TestAndSetBit( uint packedBlockPos )
{
	const uint key = ComputeKeyFromPackedBlockPos( packedBlockPos );

	const uint chunk = key >> 5;
	const uint bit = key & 0x1F;
	const uint bitMask = 1 << bit;

	if ( streamBits[chunk] & bitMask )
		return true;

	streamBits[chunk] |= bitMask;
	return false;
}

int main()
{
	V6_MSG( "Sort 0.0\n" );

	core::CHeap heap;
	core::Stack stack( &heap, 100 * 1024 * 1024 );

	V6_MSG( "Blocks:");
	for ( uint blockID = 0; blockID < (uint)block_count( GRID_CELL_BUCKET ); )
	{
		const uint mip = rand() & 0xF;
		const uint x = rand() & HLSL_GRID_MACRO_MASK;
		const uint y = rand() & HLSL_GRID_MACRO_MASK;
		const uint z = rand() & HLSL_GRID_MACRO_MASK;
		const uint blockPos = (z << HLSL_GRID_MACRO_2XSHIFT) | (y << HLSL_GRID_MACRO_SHIFT) | x;
		const uint packedBlockPos = (mip << 28) | blockPos;
		if ( TestAndSetBit( packedBlockPos ) )
			continue;

		blockPositions[blockID] = packedBlockPos;
		if ( blockID < 16 )
			V6_PRINT( " 0x%08X", packedBlockPos );

		++blockID;
	}
	V6_PRINT( "\n" );

	core::Signal_Create( &s_threadGroupDone );
	for ( uint barrierID = 0; barrierID < 2; ++barrierID )
		core::Signal_Create( &s_threadGroupBarriers[barrierID] );

	Dispatch< HLSL_STREAM_THREAD_GROUP_SIZE >( block_count( GRID_CELL_BUCKET ), SetBit, "SetBit" );

	uint elementCount = HLSL_STREAM_SIZE;
	c_streamLowerOffset = 0;
	for ( uint layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
	{
		V6_ASSERT( elementCount > 0 );
		c_streamCurrentOffset = c_streamLowerOffset;
		c_streamLowerOffset += elementCount;
		Dispatch< HLSL_STREAM_THREAD_GROUP_SIZE >( elementCount, PrefixSum, "PrefixSum" );
	}
	V6_ASSERT( elementCount <= 1 );

	Dispatch< HLSL_STREAM_THREAD_GROUP_SIZE >( HLSL_STREAM_SIZE, Summarize, "Summarize" );

	Dispatch< HLSL_STREAM_THREAD_GROUP_SIZE >( block_count( GRID_CELL_BUCKET ), Scatter, "Scatter" );

	core::Signal_Release( &s_threadGroupDone );
	for ( uint barrierID = 0; barrierID < 2; ++barrierID )
		core::Signal_Release( &s_threadGroupBarriers[barrierID] );

	qsort( blockPositions, block_count( GRID_CELL_BUCKET ), 4, CompareBlockPositionByKey );

	for ( uint blockID = 0; blockID < (uint)block_count( GRID_CELL_BUCKET ); ++blockID )
		V6_ASSERT( blockPositions[blockID] == sortedBlockPositions[blockID] );

	return 0;
}