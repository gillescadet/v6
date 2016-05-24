/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/time.h>

BEGIN_V6_NAMESPACE

const static double s_dInvFrequency = 1.0 / GetTickFrequency();

double GetTickFrequency()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return double( li.QuadPart );
}

u64 GetTickCount()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

float ConvertTicksToSeconds( u64 nTickCount )
{	
	return (float)( nTickCount * s_dInvFrequency );
}

END_V6_NAMESPACE
