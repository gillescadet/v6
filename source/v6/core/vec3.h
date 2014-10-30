/*V6*/

#pragma once

#ifndef __V6_CORE_VEC3_H__
#define __V6_CORE_VEC3_H__

BEGIN_V6_CORE_NAMESPACE

struct SVec3
{
public:
	union
	{
		struct
		{
			float m_fX;
			float m_fY;
			float m_fZ;
		};
		float m_fValues[3];
	};	
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_VEC3_H__