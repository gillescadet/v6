/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#pragma warning( pop )

#include <v6/core/common.h>
#include <v6/core/thread.h>

BEGIN_V6_CORE_NAMESPACE

u64				g_jobID							= 0;
JobBackend_s	g_jobBackends[JOB_BUFFER_SIZE]	= {};

u64 Atomic_Inc( u64* v )
{
	return InterlockedIncrement( v )-1;
}

void Thread_Create( unsigned long (__stdcall *process)( void* ), void* ctx )
{
	CreateThread( nullptr, 0, process, ctx, 0, nullptr );
}

END_V6_CORE_NAMESPACE
