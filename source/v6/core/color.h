/*V6*/

#pragma once

#ifndef __V6_CORE_COLOR_H__
#define __V6_CORE_COLOR_H__

#include <v6/core/types.h>

BEGIN_V6_CORE_NAMESPACE

struct SColor
{
	u8 m_r;
	u8 m_g;
	u8 m_b;
	u8 m_a;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_COLOR_H__