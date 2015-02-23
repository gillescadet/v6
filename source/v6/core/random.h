/*V6*/

#pragma once

#ifndef __V6_CORE_RANDOM_H__
#define __V6_CORE_RANDOM_H__

BEGIN_V6_CORE_NAMESPACE

float RandFloat()
{
	static float fInvRandMax = 1.0f / RAND_MAX;
	return rand() * fInvRandMax;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_RANDOM_H__