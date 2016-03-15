/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#pragma warning( pop )

#include <v6/core/common.h>
#include <v6/core/thread.h>

BEGIN_V6_CORE_NAMESPACE

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

END_V6_CORE_NAMESPACE
