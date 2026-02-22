/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/thread.h>

BEGIN_V6_NAMESPACE

#define WORKER_THREAD_EXIT_CODE 0xFFFFFFFF

struct WorkerJobWithFixedArgs_t
{
	WorkerProcessWithFixedArgs_f	process;
	void*							ctx;
	u32								arg0;
	u32								arg1;
};
V6_STATIC_ASSERT( sizeof( WorkerJobWithFixedArgs_t ) <= sizeof( WorkerJob_t::args ) );

static unsigned long __stdcall WorkerThreadLoop( void* workerThreadPointer )
{
	WorkerThread_s* workerThread = (WorkerThread_s*)workerThreadPointer;

	for (;;)
	{
		const u64 lastJob = Atomic_Load( &workerThread->lastJob );
		
		if ( lastJob == WORKER_THREAD_EXIT_CODE )
		{
			workerThread->firstJob = WORKER_THREAD_EXIT_CODE;
			return 0;
		}

		if ( workerThread->firstJob == lastJob )
		{
			// Thread_Switch();
			Thread_Sleep( 1 );
			continue;
		}

		WorkerJob_t* job = &workerThread->jobs[workerThread->firstJob & JOB_BUFFER_MASK];

		job->process( job->args );
		job->process = nullptr;

		V6_WRITE_BARRIER();

		++workerThread->firstJob;
	}

	return 0;
}

static void WorkerThreadProcessWithFixedArgs( const void* argData )
{
	WorkerJobWithFixedArgs_t* workerJob = (WorkerJobWithFixedArgs_t*)argData;
	workerJob->process( workerJob->ctx, workerJob->arg0, workerJob->arg1 );
}

static void WorkerThread_ClearQueue( void* workerThreadPointer, u32, u32 )
{
	WorkerThread_s* workerThread = (WorkerThread_s*)workerThreadPointer;

	u64 lastJob = Atomic_Load( &workerThread->lastJob );
	V6_ASSERT( workerThread->firstJob < lastJob );
	--lastJob;

	while ( workerThread->firstJob < lastJob )
	{
		WorkerJob_t* job = &workerThread->jobs[workerThread->firstJob & JOB_BUFFER_MASK];

		job->process = nullptr;

		V6_WRITE_BARRIER();

		++workerThread->firstJob;
	}
}

u32 Atomic_Add( u32* v, u32 inc )
{
	return InterlockedExchangeAdd( v, inc );
}

u64 Atomic_Add( u64* v, u64 inc )
{
	return InterlockedExchangeAdd( v, inc );
}

u32 Atomic_Dec( u32* v )
{
	return InterlockedDecrement( v )+1;
}

u64 Atomic_Dec( u64* v )
{
	return InterlockedDecrement( v )+1;
}

u32 Atomic_Inc( u32* v )
{
	return InterlockedIncrement( v )-1;
}

u64 Atomic_Inc( u64* v )
{
	return InterlockedIncrement( v )-1;
}

u32 Atomic_And( u32* v, u32 inc )
{
	V6_STATIC_ASSERT( sizeof( u32 ) == sizeof( long ) );
	return InterlockedAnd( (long*)v, (long)inc );
}

u64 Atomic_And( u64* v, u64 inc )
{
	return InterlockedAnd( v, inc );
}

u32 Atomic_Or( u32* v, u32 inc )
{
	V6_STATIC_ASSERT( sizeof( u32 ) == sizeof( long ) );
	return InterlockedOr( (long*)v, (long)inc );
}

u64 Atomic_Or( u64* v, u64 inc )
{
	return InterlockedOr( v, inc );
}

u32 Atomic_Min( u32* ref, u32 value )
{
	u32 refValue = *ref;
	if ( value < refValue )
	{
		const u32 prevValue = InterlockedCompareExchange( ref, value, refValue );
		if ( prevValue == refValue )
			return refValue;
		refValue = prevValue;
	}

	return refValue;
}

u64 Atomic_Min( u64* ref, u64 value )
{
	u64 refValue = *ref;
	if ( value < refValue )
	{
		const u64 prevValue = InterlockedCompareExchange( ref, value, refValue );
		if ( prevValue == refValue )
			return refValue;
		refValue = prevValue;
	}

	return refValue;
}

u32 Atomic_Max( u32* ref, u32 value )
{
	u32 refValue = *ref;
	if ( value > refValue )
	{
		const u32 prevValue = InterlockedCompareExchange( ref, value, refValue );
		if ( prevValue == refValue )
			return refValue;
		refValue = prevValue;
	}

	return refValue;
}

u64 Atomic_Max( u64* ref, u64 value )
{
	u64 refValue = *ref;
	if ( value > refValue )
	{
		const u64 prevValue = InterlockedCompareExchange( ref, value, refValue );
		if ( prevValue == refValue )
			return refValue;
		refValue = prevValue;
	}

	return refValue;
}

u32 Atomic_Set( u32* v, u32 setValue )
{
	return InterlockedExchange( v, setValue );
}

u64 Atomic_Set( u64* v, u64 setValue )
{
	return InterlockedExchange( v, setValue );
}

void Mutex_Create( Mutex_s* mutex )
{
	mutex->handle = CreateMutex( nullptr, false, nullptr );
}

void Mutex_Lock( Mutex_s* mutex )
{
	WaitForSingleObject( mutex->handle, INFINITE );
}

void Mutex_Unlock( Mutex_s* mutex )
{
	ReleaseMutex( mutex->handle );
}

void Mutex_Release( Mutex_s* mutex )
{
	CloseHandle( mutex->handle );
}

void Signal_Create( Signal_s* signal )
{
	signal->handle = CreateEvent( nullptr, true, false, nullptr );
}

void Signal_Emit( Signal_s* signal )
{
	SetEvent( signal->handle );
}

void Signal_Reset( Signal_s* signal )
{
	ResetEvent( signal->handle );
}

void Signal_Release( Signal_s* signal )
{
	CloseHandle( signal->handle );
}

void Signal_Wait( Signal_s* signal )
{
	WaitForSingleObject( signal->handle, INFINITE );
}

void Thread_Create( unsigned long (__stdcall *process)( void* ), void* ctx, u32 core )
{
	HANDLE thread = CreateThread( nullptr, 0, process, ctx, 0, nullptr );
	if ( core != THREAD_ANY_CORE )
	{
		const DWORD_PTR oldMask = SetThreadAffinityMask( thread, 1ll << core );
		V6_ASSERT( oldMask != 0 );
	}
}

void Thread_SetCoreAffinity( u32 core )
{
	const HANDLE thread = GetCurrentThread();
	const DWORD_PTR oldMask = SetThreadAffinityMask( thread, 1ll << core );
	V6_ASSERT( oldMask != 0 );
}

u32 Thread_GetCurrentCore()
{
	return (u32)GetCurrentProcessorNumber();
}

void Thread_Sleep( u32 ms )
{
	Sleep( ms );
}

void Thread_Switch()
{
	SwitchToThread();
}

void WorkerThread_Create( WorkerThread_s* workerThread, u32 core )
{
	memset( workerThread, 0, sizeof( *workerThread) );
	Thread_Create( WorkerThreadLoop, workerThread, core );
}

void WorkerThread_AddJob( WorkerThread_s* workerThread, WorkerProcess_f process, void* argData, u32 argSize )
{
	V6_ASSERT( process != nullptr );
	V6_ASSERT( workerThread->lastJob - workerThread->firstJob < JOB_BUFFER_SIZE );
	V6_ASSERT( argSize <= sizeof( WorkerJob_t::args ) );
	const u32 jobID = workerThread->lastJob & JOB_BUFFER_MASK;
	V6_ASSERT( workerThread->jobs[jobID].process == nullptr );
	workerThread->jobs[jobID].process = process;
	memcpy( workerThread->jobs[jobID].args, argData, argSize );
	V6_WRITE_BARRIER();
	++workerThread->lastJob;
}

void WorkerThread_AddJob( WorkerThread_s* workerThread, WorkerProcessWithFixedArgs_f process, void* context, u32 arg0, u32 arg1 )
{
	WorkerJobWithFixedArgs_t argData;
	argData.process = process;
	argData.ctx = context;
	argData.arg0 = arg0;
	argData.arg1 = arg1;
	WorkerThread_AddJob( workerThread, WorkerThreadProcessWithFixedArgs, &argData, sizeof( argData ) ) ;

}

u32 WorkerThread_GetPendingJobCount( WorkerThread_s* workerThread )
{
	return (u32)(workerThread->lastJob - Atomic_Load( &workerThread->firstJob ));
}

void WorkerThread_CancelAllJobs( WorkerThread_s* workerThread )
{
	if ( Atomic_Load( &workerThread->firstJob ) == workerThread->lastJob )
		return;

	WorkerThread_AddJob( workerThread, WorkerThread_ClearQueue, workerThread, 0 , 0 );
	
	while ( Atomic_Load( &workerThread->firstJob ) != workerThread->lastJob )
		Thread_Switch();
}

void WorkerThread_WaitAllJobs( WorkerThread_s* workerThread )
{
	while ( Atomic_Load( &workerThread->firstJob ) < workerThread->lastJob )
		Thread_Switch();
}

void WorkerThread_Release( WorkerThread_s* workerThread )
{
	V6_ASSERT( workerThread->firstJob == workerThread->lastJob );
	workerThread->lastJob = WORKER_THREAD_EXIT_CODE;
	while ( Atomic_Load( &workerThread->firstJob ) != WORKER_THREAD_EXIT_CODE )
		Thread_Switch();
}

END_V6_NAMESPACE
