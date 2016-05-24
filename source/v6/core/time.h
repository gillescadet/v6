/*V6*/

#pragma once

#ifndef __V6_CORE_TIME_H__
#define __V6_CORE_TIME_H__

BEGIN_V6_NAMESPACE

float		ConvertTicksToSeconds( u64 nTickCount );
u64			GetTickCount();
double		GetTickFrequency();

END_V6_NAMESPACE

#endif // __V6_CORE_TIME_H__