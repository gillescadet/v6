/*V6*/

#pragma once

#ifndef __V6_CORE_KDTREE_SPHERE_H__
#define __V6_CORE_KDTREE_SPHERE_H__

#include <v6/core/vec3.h>

BEGIN_V6_CORE_NAMESPACE

class CBlockAllocator;
class IAllocator;

class CKDTreeSphere
{
public:
	struct SSPhere
	{
		Vec3 m_vPoint;
		float m_fRadius;
	};

public:
	CKDTreeSphere(IAllocator & oHeap);
	~CKDTreeSphere();

public:
	void Build(SSPhere const * pSpheres, int nSphereCount);

private:
	IAllocator & m_oHeap;
	CBlockAllocator * m_pBlockAllocator;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_KDTREE_SPHERE_H__