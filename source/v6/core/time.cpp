/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/math.h>
#include <v6/core/thread.h>
#include <v6/core/time.h>

#define V6_USE_VTUNE 0

#if V6_USE_VTUNE == 1

#include "C:/Program Files (x86)/IntelSWTools/VTune Amplifier XE 2017/include/ittnotify.h"
#pragma comment( lib, "C:/Program Files (x86)/IntelSWTools/VTune Amplifier XE 2017/lib64/libittnotify.lib" )

#endif // #if V6_USE_VTUNE == 1

BEGIN_V6_NAMESPACE

const static double s_dInvFrequency = 1.0 / Tick_GetFrequency();

static const u32 CPU_EVENT_MAX_COUNT			= 32;
static const u32 CPU_EVENT_NAME_MAX_SIZE		= 64;
static const u32 CPU_EVENT_STACK_MAX_SIZE		= 32;
static const u32 CPU_EVENT_TIMING_FRAME_COUNT	= 32;

#if V6_USE_VTUNE == 1

struct CPUEventSharedContext_s
{
	__itt_string_handle*	eventHandles[CPU_EVENT_MAX_COUNT];
	b8						eventProfiles[CPU_EVENT_MAX_COUNT];
	u32						eventCount;
};

static __itt_domain* s_ittDomain = __itt_domain_createA( "v6" );

#else

struct CPUEventSharedContext_s
{
	char				eventNames[CPU_EVENT_MAX_COUNT][CPU_EVENT_NAME_MAX_SIZE];
	b8					eventProfiles[CPU_EVENT_MAX_COUNT];
	u64					event_callCount16_duration48_array[CPU_EVENT_MAX_COUNT];
	u32					eventCount;

	struct
	{
		u32				durations[CPU_EVENT_MAX_COUNT][CPU_EVENT_TIMING_FRAME_COUNT];
		u64				durationSums[CPU_EVENT_MAX_COUNT];
		u32				callCounts[CPU_EVENT_MAX_COUNT][CPU_EVENT_TIMING_FRAME_COUNT];
		u32				callCountSums[CPU_EVENT_MAX_COUNT];
		u32				updateID;
	}					timings;

	CPUEventDuration_s	eventDurations[CPU_EVENT_MAX_COUNT];
};

struct CPUEventThreadContext_s
{
	u32				stackIDs[CPU_EVENT_STACK_MAX_SIZE];
	u64				stackStartTimes[CPU_EVENT_STACK_MAX_SIZE];
	u64				stackChildrenDurations[CPU_EVENT_STACK_MAX_SIZE];
	u32				stackSize;
};

static V6_THREAD_LOCAL_STORAGE CPUEventThreadContext_s		s_eventThreadContext;

#endif

static CPUEventSharedContext_s								s_eventSharedContext;


static int CPUEventDuration_Compare( const void* eventDurationPointer0, const void* eventDurationPointer1 )
{
	const CPUEventDuration_s* eventDuration0 = (CPUEventDuration_s*)eventDurationPointer0;
	const CPUEventDuration_s* eventDuration1 = (CPUEventDuration_s*)eventDurationPointer1;

	if ( eventDuration0->avgDurationUS == eventDuration1->avgDurationUS )
		return 0;

	return eventDuration0->avgDurationUS > eventDuration1->avgDurationUS ? -1 : 1;
}


double Tick_GetFrequency()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return double( li.QuadPart );
}

u64 Tick_GetCount()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

float Tick_ConvertToSeconds( u64 nTickCount )
{	
	return (float)( nTickCount * s_dInvFrequency );
}

CPUEventID_t CPUEvent_Find( const char* eventName )
{
	for ( u32 eventID = 0; eventID < Atomic_Load( &s_eventSharedContext.eventCount ); ++eventID )
	{
		if ( strcmp( s_eventSharedContext.eventNames[eventID], eventName ) == 0 )
			return (CPUEventID_t)eventID;
	}

	return (CPUEventID_t)-1;
}

CPUEventID_t CPUEvent_Register( const char* eventName, bool profile )
{
	const CPUEventID_t eventID = Atomic_Inc( &s_eventSharedContext.eventCount );
	V6_ASSERT( eventID < CPU_EVENT_MAX_COUNT );

#if V6_USE_VTUNE == 1

	s_eventSharedContext.eventHandles[eventID] = __itt_string_handle_createA( eventName );

#else

	strcpy_s( s_eventSharedContext.eventNames[eventID], CPU_EVENT_NAME_MAX_SIZE-1, eventName );
	s_eventSharedContext.event_callCount16_duration48_array[eventID] = 0;

#endif

	s_eventSharedContext.eventProfiles[eventID] = profile;

	return eventID;
}

void CPUEvent_Begin( CPUEventID_t eventID )
{
	V6_ASSERT( eventID < Atomic_Load( &s_eventSharedContext.eventCount ) );

#if V6_USE_VTUNE == 1

	__itt_task_begin( s_ittDomain, __itt_null, __itt_null, s_eventSharedContext.eventHandles[eventID] );

#else

	const u32 depth = s_eventThreadContext.stackSize;
	V6_ASSERT( depth < CPU_EVENT_STACK_MAX_SIZE );
	
	++s_eventThreadContext.stackSize;
	
	s_eventThreadContext.stackIDs[depth] = eventID;
	s_eventThreadContext.stackStartTimes[depth] = Tick_GetCount(); // last operation
	s_eventThreadContext.stackChildrenDurations[depth] = 0;

#endif
}

void CPUEvent_End()
{
#if V6_USE_VTUNE == 1

	__itt_task_end( s_ittDomain );

#else

	const u64 endTime = Tick_GetCount(); // first operation

	V6_ASSERT( s_eventThreadContext.stackSize > 0 );

	--s_eventThreadContext.stackSize;
	const u32 depth = s_eventThreadContext.stackSize;

	const CPUEventID_t eventID = s_eventThreadContext.stackIDs[depth];
	const u64 fullDuration = endTime - s_eventThreadContext.stackStartTimes[depth];
	V6_ASSERT( fullDuration >= s_eventThreadContext.stackChildrenDurations[depth] );
	const u64 selfDuration = fullDuration - s_eventThreadContext.stackChildrenDurations[depth];
	if ( depth > 0 )
		s_eventThreadContext.stackChildrenDurations[depth-1] += fullDuration;
	V6_ASSERT( selfDuration < (1ll << 48) );
	const u64 callCount16_duration48 = (1ll << 48) | selfDuration;
	Atomic_Add( &s_eventSharedContext.event_callCount16_duration48_array[eventID], callCount16_duration48 );

#endif
}

u32 CPUEvent_UpdateDurations( CPUEventDuration_s** eventDurations )
{
#if V6_USE_VTUNE == 1
	return 0;
#else
	u32 eventDurationCount = 0;
	const u32 timingFrameID = s_eventSharedContext.timings.updateID % CPU_EVENT_TIMING_FRAME_COUNT;
	for ( u32 eventID = 0; eventID < s_eventSharedContext.eventCount; ++eventID )
	{
		if ( !s_eventSharedContext.eventProfiles[eventID] )
			continue;

		const u64 callCount16_duration48 = Atomic_Set( &s_eventSharedContext.event_callCount16_duration48_array[eventID], 0ll );
		const u32 callCount = callCount16_duration48 >> 48;

		if ( callCount == 0 )
			continue;

		const u64 duration = callCount16_duration48 & ((1ll << 48) - 1);

		const float time = Tick_ConvertToSeconds( duration );
		const u32 timeUS = (u32)(Min( time, 1.0f ) * 1000000.0f);

		s_eventSharedContext.timings.callCountSums[eventID] -= s_eventSharedContext.timings.callCounts[eventID][timingFrameID];
		s_eventSharedContext.timings.callCounts[eventID][timingFrameID] = callCount;
		s_eventSharedContext.timings.callCountSums[eventID] += callCount;

		s_eventSharedContext.timings.durationSums[eventID] -= s_eventSharedContext.timings.durations[eventID][timingFrameID];
		s_eventSharedContext.timings.durations[eventID][timingFrameID] = timeUS;
		s_eventSharedContext.timings.durationSums[eventID] += timeUS;

		CPUEventDuration_s* eventDuration = &s_eventSharedContext.eventDurations[eventDurationCount];
		eventDuration->id = (CPUEventID_t)eventID;
		eventDuration->name = s_eventSharedContext.eventNames[eventID];
		eventDuration->avgDurationUS = (u32)(s_eventSharedContext.timings.durationSums[eventID] / CPU_EVENT_TIMING_FRAME_COUNT);
		eventDuration->avgCallCount = s_eventSharedContext.timings.callCountSums[eventID] * (1.0f / CPU_EVENT_TIMING_FRAME_COUNT);
		eventDuration->curCallCount = callCount;
		eventDuration->curDurationUS = timeUS;

		++eventDurationCount;
	}

	qsort( s_eventSharedContext.eventDurations, eventDurationCount, sizeof( s_eventSharedContext.eventDurations[0] ), CPUEventDuration_Compare );

	*eventDurations = s_eventSharedContext.eventDurations;

	++s_eventSharedContext.timings.updateID;

	return eventDurationCount;
#endif
}

END_V6_NAMESPACE
