/*V6*/

#pragma once

#ifndef __V6_CORE_THREAD_H__
#define __V6_CORE_THREAD_H__

BEGIN_V6_NAMESPACE

#define JOB_BUFFER_SIZE		32
#define JOB_BUFFER_MASK		(JOB_BUFFER_SIZE-1)

template  < typename T >
struct Job_s
{
	typedef void	(*Process_f)( T* ctx );

	Process_f		process;
	T*				context;	
};

struct JobBackend_s
{
	void*			process;
	void*			context;
};

struct Signal_s
{
	void*			handle;
};

typedef Job_s< void >				WorkerJob_t;
typedef Job_s< void >::Process_f	WorkerProcess_f;

struct WorkerThread_s
{
	WorkerJob_t		jobs[JOB_BUFFER_SIZE];
	u64				firstJob;
	u64				lastJob;
};

u32			Atomic_Add( u32* v, u32 inc );
u64			Atomic_Add( u64* v, u64 inc );
u32			Atomic_And( u32* v, u32 mask );
u64			Atomic_And( u64* v, u64 mask );
u32			Atomic_Dec( u32* v );
u64			Atomic_Dec( u64* v );
u32			Atomic_Inc( u32* v );
u64			Atomic_Inc( u64* v );
template < typename T >
T			Atomic_Load( T* p ) { return *((volatile T*)p); }
u32			Atomic_Or( u32* v, u32 mask );
u64			Atomic_Or( u64* v, u64 mask );

template  < typename T >
void Job_Launch( typename Job_s< T >::Process_f process,  T* context );

void Signal_Create( Signal_s* signal );
void Signal_Emit( Signal_s* signal );
void Signal_Reset( Signal_s* signal );
void Signal_Release( Signal_s* signal );
void Signal_Wait( Signal_s* signal );

void Thread_Create( unsigned long (__stdcall *process)( void* ), void* ctx );

void WorkerThread_Create( WorkerThread_s* workerThread );
void WorkerThread_AddJob( WorkerThread_s* workerThread, WorkerProcess_f process, void* context );
void WorkerThread_WaitAllJobs( WorkerThread_s* workerThread );
void WorkerThread_Release( WorkerThread_s* workerThread );

extern u64				g_jobCount;
extern JobBackend_s		g_jobBackends[JOB_BUFFER_SIZE];

template  < typename T >
unsigned long __stdcall __Job_Execute( void* jobPointer )
{
	Job_s< T >* job = (Job_s< T >*)jobPointer;
	Job_s< T >::Process_f process = job->process;
	T* context = job->context;
	memset( job, 0, sizeof( Job_s< T > ) );
	
	process( context );

	return 0; 
}

template  < typename T >
void Job_Launch( typename Job_s< T >::Process_f process,  T* context )
{
	const u32 jobID = Atomic_Inc( &g_jobCount ) & (JOB_BUFFER_SIZE - 1);
	V6_ASSERT( jobID < JOB_BUFFER_SIZE );
	V6_ASSERT( g_jobBackends[jobID].process == nullptr );
	V6_ASSERT( sizeof(  JobBackend_s ) == sizeof( Job_s< T > ) );
	Job_s< T >* job = (Job_s< T >*)&g_jobBackends[jobID];
	job->process = process;
	job->context = context;
	Thread_Create( __Job_Execute< T >, job );
}

END_V6_NAMESPACE

#endif // __V6_CORE_THREAD_H__