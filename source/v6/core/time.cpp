/*V6*/

#include <v6/core/common.h>
#include <v6/core/time.h>

#include <windows.h>

BEGIN_V6_CORE_NAMESPACE

static double g_fPCFreq = 0.0;

double GetTickFrequency()
{
	LARGE_INTEGER li;
	BOOL bRes = QueryPerformanceFrequency(&li);
	V6_ASSERT(bRes);
	return double(li.QuadPart);
}

__int64 GetTickCount()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

float ConvertTicksToSeconds(__int64 nTickCount)
{
	static double dInvFrequency = 1.0 / GetTickFrequency();
	return (float)(nTickCount * dInvFrequency);
}

END_V6_CORE_NAMESPACE