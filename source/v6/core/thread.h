/*V6*/

#pragma once

#ifndef __V6_CORE_THREAD_H__
#define __V6_CORE_THREAD_H__

BEGIN_V6_NAMESPACE

#define V6_READ_BARRIER			_ReadBarrier
#define V6_WRITE_BARRIER		_WriteBarrier
#define V6_READ_WRITE_BARRIER	_ReadWriteBarrier

#define JOB_BUFFER_SIZE		1024
#define JOB_BUFFER_MASK		(JOB_BUFFER_SIZE-1)

#define THREAD_ANY_CORE		0xFFFFFFFF

typedef void (*WorkerProcess_f)( const void* argData );
typedef void (*WorkerProcessWithFixedArgs_f)( void* ctx, u32 arg0, u32 arg1 );

struct WorkerJob_t
{
	WorkerProcess_f	process;
	char			args[56];
};
V6_STATIC_ASSERT( sizeof( WorkerJob_t ) == 64 );

struct Mutex_s
{
	void*			handle;
};

struct Signal_s
{
	void*			handle;
};

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
u32			Atomic_Max( u32* ref, u32 value );
u64			Atomic_Max( u64* ref, u64 value );
u32			Atomic_Min( u32* ref, u32 value );
u64			Atomic_Min( u64* ref, u64 value );
u32			Atomic_Or( u32* v, u32 mask );
u64			Atomic_Or( u64* v, u64 mask );
u32			Atomic_Set( u32* v, u32 setValue );
u64			Atomic_Set( u64* v, u64 setValue );

void		Mutex_Create( Mutex_s* mutex );
void		Mutex_Lock( Mutex_s* mutex );
void		Mutex_Unlock( Mutex_s* mutex );
void		Mutex_Release( Mutex_s* mutex );

void		Signal_Create( Signal_s* signal );
void		Signal_Emit( Signal_s* signal );
void		Signal_Reset( Signal_s* signal );
void		Signal_Release( Signal_s* signal );
void		Signal_Wait( Signal_s* signal );

void		Thread_Create( unsigned long (__stdcall *process)( void* ), void* ctx, u32 core );
u32			Thread_GetCurrentCore();
void		Thread_SetCoreAffinity( u32 core );
void		Thread_Sleep( u32 ms );
void		Thread_Switch();

void		WorkerThread_CancelAllJobs( WorkerThread_s* workerThread );
void		WorkerThread_Create( WorkerThread_s* workerThread, u32 core );
void		WorkerThread_AddJob( WorkerThread_s* workerThread, WorkerProcess_f process, void* argData, u32 argSize );
void		WorkerThread_AddJob( WorkerThread_s* workerThread, WorkerProcessWithFixedArgs_f process, void* context, u32 arg0, u32 arg1 );
u32			WorkerThread_GetPendingJobCount( WorkerThread_s* workerThread );
void		WorkerThread_WaitAllJobs( WorkerThread_s* workerThread );
void		WorkerThread_Release( WorkerThread_s* workerThread );

END_V6_NAMESPACE

#endif // __V6_CORE_THREAD_H__