/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#pragma warning( pop )

#include <v6/core/common.h>
#include <v6/core/time.h>

BEGIN_V6_NAMESPACE

const static double s_dInvFrequency = 1.0 / GetTickFrequency();

double GetTickFrequency()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return double( li.QuadPart );
}

__int64 GetTickCount()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

float ConvertTicksToSeconds(__int64 nTickCount)
{	
	return (float)( nTickCount * s_dInvFrequency );
}

END_V6_NAMESPACE
