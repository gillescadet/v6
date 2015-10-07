/*V6*/

#include <v6/core/common.h>
#include <v6/core/kdtree_sphere.h>

#include <v6/core/memory.h>

BEGIN_V6_CORE_NAMESPACE

struct SKDTreeSphereBuildState
{
	int * m_pSphereIds[3];
};

CKDTreeSphere::CKDTreeSphere(IAllocator & oHeap)
: m_oHeap(oHeap)
, m_pBlockAllocator(oHeap.newInstance<core::CBlockAllocator>(oHeap))
{

}

CKDTreeSphere::~CKDTreeSphere()
{
	m_oHeap.deleteInstance(m_pBlockAllocator);
}

void CKDTreeSphere::Build(SSPhere const * pSpheres, int nSphereCount)
{
	SKDTreeSphereBuildState * pState = m_pBlockAllocator->newInstance<SKDTreeSphereBuildState>();
	for (int nAxis = 0; nAxis < 3; ++nAxis)
	{
		int * pSphereIds = (int *)m_pBlockAllocator->alloc(nSphereCount * sizeof(int));
		for (int nId = 0; nId < nSphereCount; ++nId)
		{
			pSphereIds[nId] = nId;
		}
		
		pState->m_pSphereIds[nAxis] = pSphereIds;
	}
}

END_V6_CORE_NAMESPACE