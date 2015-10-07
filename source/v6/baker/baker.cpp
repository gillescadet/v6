/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/baker.h>

#include <v6/core/filesystem.h>
#include <v6/core/memory.h>

BEGIN_V6_BAKER_NAMESPACE

CBaker::CBaker(core::IAllocator & oHeap)
	: m_oHeap(oHeap)
	, m_pFileSystem(oHeap.newInstance<core::CFileSystem>())
{	
}

CBaker::~CBaker()
{
	m_oHeap.deleteInstance(m_pFileSystem);
}

END_V6_BAKER_NAMESPACE