/*V6*/

#pragma once

#ifndef __V6_CORE_THREAD_H__
#define __V6_CORE_THREAD_H__

BEGIN_V6_CORE_NAMESPACE

#define JOB_BUFFER_SIZE		32

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

u64 Atomic_Inc( u64* v );

template  < typename T >
void Job_Launch( typename Job_s< T >::Process_f process,  T* context );

void Thread_Create( unsigned long (__stdcall *process)( void* ), void* ctx );

extern u64				g_jobID;
extern JobBackend_s		g_jobBackends[JOB_BUFFER_SIZE];

template  < typename T >
DWORD __stdcall __Job_Execute( void* jobPointer )
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
	const u32 jobID = Atomic_Inc( &g_jobID ) & (JOB_BUFFER_SIZE - 1);
	V6_ASSERT( g_jobBackends[jobID].process == nullptr );
	V6_ASSERT( sizeof(  JobBackend_s ) == sizeof( Job_s< T > ) );
	Job_s< T >* job = (Job_s< T >*)&g_jobBackends[jobID];
	job->process = process;
	job->context = context;
	Thread_Create( __Job_Execute< T >, job );
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_THREAD_H__