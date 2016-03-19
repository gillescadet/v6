/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#pragma warning( pop )

#include <v6/core/common.h>
#include <v6/core/thread.h>

BEGIN_V6_CORE_NAMESPACE

#define WORKER_THREAD_EXIT_CODE 0xFFFFFFFF

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
			SwitchToThread();
			continue;
		}

		if ( lastJob == 0xFFFFFFFF )
		const u32 jobID = workerThread->firstJob & JOB_BUFFER_MASK;
		WorkerJob_t* job = &workerThread->jobs[workerThread->firstJob & JOB_BUFFER_MASK];

		job->process( job->context );

		job->process = nullptr;
		job->context = nullptr;

		++workerThread->firstJob;
	}

	return 0;
}

u64				g_jobCount						= 0;
JobBackend_s	g_jobBackends[JOB_BUFFER_SIZE]	= {};

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

void Thread_Create( unsigned long (__stdcall *process)( void* ), void* ctx )
{
	CreateThread( nullptr, 0, process, ctx, 0, nullptr );
}

void WorkerThread_Create( WorkerThread_s* workerThread )
{
	memset( workerThread, 0, sizeof( *workerThread) );
	Thread_Create( WorkerThreadLoop, workerThread );
}

void WorkerThread_AddJob( WorkerThread_s* workerThread, WorkerProcess_f process, void* context )
{
	V6_ASSERT( process != nullptr );
	V6_ASSERT( workerThread->lastJob - workerThread->firstJob < JOB_BUFFER_SIZE );
	const u32 jobID = workerThread->lastJob & JOB_BUFFER_MASK;
	V6_ASSERT( workerThread->jobs[jobID].process == nullptr );
	workerThread->jobs[jobID].process = process;
	workerThread->jobs[jobID].context = context;
	++workerThread->lastJob;
}

void WorkerThread_WaitAllJobs( WorkerThread_s* workerThread )
{
	while ( Atomic_Load( &workerThread->firstJob ) < workerThread->lastJob )
		SwitchToThread();
}

void WorkerThread_Release( WorkerThread_s* workerThread )
{
	V6_ASSERT( workerThread->firstJob == workerThread->lastJob );
	workerThread->lastJob = WORKER_THREAD_EXIT_CODE;
	while ( Atomic_Load( &workerThread->firstJob ) != WORKER_THREAD_EXIT_CODE )
		SwitchToThread();
}

END_V6_CORE_NAMESPACE
