/*V6*/

#pragma once

#ifndef __V6_CORE_COLOR_H__
#define __V6_CORE_COLOR_H__

BEGIN_V6_NAMESPACE

struct Color_s
{
	union
	{
		struct
		{
			u8 m_r;
			u8 m_g;
			u8 m_b;
			u8 m_a;
		};
		struct
		{
			u8 r;
			u8 g;
			u8 b;
			u8 a;
		};
		u32 bits;
	};
};

V6_INLINE Color_s Color_Make( u8 r, u8 g, u8 b, u8 a )
{
	Color_s c;
	c.m_r = r;
	c.m_g = g;
	c.m_b = b;
	c.m_a = a;

	return c;
}

V6_INLINE Color_s Color_Black()			{ return Color_Make(   0,   0,   0, 255 );  }
V6_INLINE Color_s Color_Blue()			{ return Color_Make(   0,   0, 255, 255 );  }
V6_INLINE Color_s Color_Gray()			{ return Color_Make( 127, 127, 127, 255 );  }
V6_INLINE Color_s Color_Green()			{ return Color_Make(   0, 255,   0, 255 );  }
V6_INLINE Color_s Color_Red()			{ return Color_Make( 255,   0,   0, 255 );  }
V6_INLINE Color_s Color_White()			{ return Color_Make( 255, 255, 255, 255 );  }
V6_INLINE Color_s Color_Transparent()	{ return Color_Make(   0,   0,   0,   0 );  }

END_V6_NAMESPACE

#endif // __V6_CORE_COLOR_H__
