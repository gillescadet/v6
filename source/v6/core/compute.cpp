/*V6*/

#include <v6/core/common.h>
#include <v6/core/compute.h>

#include <v6/core/thread.h>

#define GROUP_MAX_SIZE	1024

BEGIN_V6_CORE_NAMESPACE

static bool		s_initialized = false;
static u32		s_threadDoneCount = 0;
static u32		s_threadGroupSize = 0;
static u32		s_barrier = 0;
static u32		s_concurrentBarrierCounts[2];
static Signal_s	s_threadGroupDone;
static Signal_s	s_threadGroupBarriers[2];

struct ThreadContext_s
{
	Compute_DispatchKernel_f	kernel;
	u32							groupID;
	u32							threadGroupID;
	u32							threadID;
};

// Local

static void KernelWrapper( ThreadContext_s* threadContext )
{
	threadContext->kernel( threadContext->groupID, threadContext->threadGroupID, threadContext->threadID );

	if ( Atomic_Inc( &s_threadDoneCount ) + 1 == s_threadGroupSize )
		Signal_Emit( &s_threadGroupDone );
}

END_V6_CORE_NAMESPACE

BEGIN_V6_HLSL_NAMESPACE

// HLSL API

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

void InterlockedAdd( uint& value, uint add )
{
	core::Atomic_Add( &value, add );
}

void InterlockedOr( uint& value, uint mask )
{
	core::Atomic_Or( &value, mask );
}

void AllMemoryBarrierWithGroupSync()
{
	const uint barrier = v6::core::s_barrier;
	if ( v6::core::Atomic_Inc( &v6::core::s_concurrentBarrierCounts[barrier] ) + 1 < v6::core::s_threadGroupSize )
	{
		v6::core::Signal_Wait( &v6::core::s_threadGroupBarriers[barrier] );
		V6_ASSERT( v6::core::s_concurrentBarrierCounts[barrier] == v6::core::s_threadGroupSize );
	}
	else
	{
		v6::core::s_barrier = 1 - barrier;
		v6::core::Signal_Reset( &v6::core::s_threadGroupBarriers[v6::core::s_barrier] );
		v6::core::s_concurrentBarrierCounts[v6::core::s_barrier] = 0;

		v6::core::Signal_Emit( &v6::core::s_threadGroupBarriers[barrier] );
	}
}

END_V6_HLSL_NAMESPACE

BEGIN_V6_CORE_NAMESPACE

// Compute API

void Compute_Init()
{
	V6_ASSERT( !s_initialized );

	Signal_Create( &s_threadGroupDone );
	for ( u32 barrierID = 0; barrierID < 2; ++barrierID )
		Signal_Create( &s_threadGroupBarriers[barrierID] );

	s_initialized = true;
}
void Compute_Release()
{
	V6_ASSERT( s_initialized );

	Signal_Release( &s_threadGroupDone );
	for ( u32 barrierID = 0; barrierID < 2; ++barrierID )
		Signal_Release( &s_threadGroupBarriers[barrierID] );

	s_initialized = false;
}

void Compute_Dispatch( u32 elementCount, u32 groupSize, Compute_DispatchKernel_f kernel, const char* name )
{
	V6_ASSERT( s_initialized );
	V6_ASSERT( groupSize < GROUP_MAX_SIZE  );
	s_threadGroupSize = groupSize;

	u32 threadID = 0;
	V6_PRINT( "Dispatch %s( %d/%d )", name, threadID, elementCount );

	for ( u32 groupID = 0; threadID < elementCount; ++groupID )
	{
		s_threadDoneCount = 0;
		Signal_Reset( &s_threadGroupDone );

		s_barrier = 0;
		Signal_Reset( &s_threadGroupBarriers[0] );
		s_concurrentBarrierCounts[0] = 0;

		ThreadContext_s threadContexts[GROUP_MAX_SIZE];
		for ( u32 threadGroupID = 0; threadGroupID < groupSize; ++threadGroupID, ++threadID )
		{
			threadContexts[threadGroupID].kernel = kernel;
			threadContexts[threadGroupID].groupID = groupID;
			threadContexts[threadGroupID].threadGroupID = threadGroupID;
			threadContexts[threadGroupID].threadID = threadID;
			Job_Launch( KernelWrapper, &threadContexts[threadGroupID] );
		}

		Signal_Wait( &s_threadGroupDone );
		
		V6_ASSERT( s_threadDoneCount == groupSize );
	}

	
	V6_PRINT( "\rDispatch %s( %d/%d )\n", name, threadID, elementCount );
}

END_V6_CORE_NAMESPACE
