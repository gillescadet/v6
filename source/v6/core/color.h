/*V6*/

#pragma once

#ifndef __V6_CORE_COLOR_H__
#define __V6_CORE_COLOR_H__

BEGIN_V6_CORE_NAMESPACE

struct SColor
{
	u8 m_r;
	u8 m_g;
	u8 m_b;
	u8 m_a;
};

V6_INLINE SColor Color_Make( u8 r, u8 g, u8 b, u8 a )
{
	SColor c;
	c.m_r = r;
	c.m_g = g;
	c.m_b = b;
	c.m_a = a;

	return c;
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_COLOR_H__