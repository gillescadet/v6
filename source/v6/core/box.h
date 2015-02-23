/*V6*/

#pragma once

#ifndef __V6_CORE_BOX_H__
#define __V6_CORE_BOX_H__

#include <v6/core/math.h>
#include <v6/core/vec3.h>

BEGIN_V6_CORE_NAMESPACE

struct SBox
{
public:
	Vec3 m_vMin;
	Vec3 m_vMax;

public:
	void Clear()
	{
		m_vMin.m_fX = FLT_MAX;
		m_vMin.m_fY = FLT_MAX;
		m_vMin.m_fZ = FLT_MAX;
		m_vMax.m_fX = -FLT_MAX;
		m_vMax.m_fY = -FLT_MAX;
		m_vMax.m_fZ = -FLT_MAX;
	}

	void Extend(Vec3 const & vPoint)
	{
		m_vMin = Min(m_vMin, vPoint);
		m_vMax = Max(m_vMax, vPoint);
	}

	void Extend(SBox const & oBox)
	{
		m_vMin = Min(m_vMin, oBox.m_vMin);
		m_vMax = Max(m_vMax, oBox.m_vMax);
	}

	Vec3 Size() const
	{
		return m_vMax - m_vMin;
	}
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_BOX_H__