/*V6*/

#pragma once

#ifndef __V6_CORE_TIME_H__
#define __V6_CORE_TIME_H__

#define V6_CPU_EVENT_SCOPE( ID ) v6::CPUEventScope_s s_cpuEventScope_##ID( ID )

BEGIN_V6_NAMESPACE

template < u32 HITCH_TIME_US >
struct ScopedHitchDetection
{
	ScopedHitchDetection( const char* nameArg )
	{ 
		startTick = Tick_GetCount();
		name = nameArg;
	}
	
	~ScopedHitchDetection()
	{ 
		const u32 timeUS = (u32)(Tick_ConvertToSeconds( Tick_GetCount() - startTick ) * 1000000);
		if ( timeUS >= HITCH_TIME_US )
			V6_MSG( "%s takes %dus\n", name, timeUS );
	}

	u64			startTick;
	const char*	name;
};
#define SCOPED_HITCH_DETECTION( NAME, MIN_TIME_US ) v6::ScopedHitchDetection< MIN_TIME_US > NAME_var( #NAME );

typedef u16 CPUEventID_t;

struct CPUEventDuration_s
{
	const char*		name;
	u32				avgDurationUS;
	float			avgCallCount;
	u32				curDurationUS;
	u32				curCallCount;
	CPUEventID_t	id;
};

void CPUEvent_Begin( CPUEventID_t eventID );
void CPUEvent_End();

struct CPUEventScope_s
{
	CPUEventScope_s( CPUEventID_t eventID )
	{ 
		CPUEvent_Begin( eventID );
	}
	
	~CPUEventScope_s()
	{ 
		CPUEvent_End();
	}
};

float			Tick_ConvertToSeconds( u64 nTickCount );
u64				Tick_GetCount();
double			Tick_GetFrequency();

void			CPUEvent_Begin( CPUEventID_t eventID );
void			CPUEvent_End();
CPUEventID_t	CPUEvent_Find( const char* eventName );
CPUEventID_t	CPUEvent_Register( const char* eventName, bool profile );
u32				CPUEvent_UpdateDurations( CPUEventDuration_s** eventDurations );

END_V6_NAMESPACE

#endif // __V6_CORE_TIME_H__