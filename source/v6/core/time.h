/*V6*/

#pragma once

#ifndef __V6_CORE_TIME_H__
#define __V6_CORE_TIME_H__

BEGIN_V6_CORE_NAMESPACE

float		ConvertTicksToSeconds( __int64 nTickCount );
__int64		GetTickCount();
double		GetTickFrequency();

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_TIME_H__