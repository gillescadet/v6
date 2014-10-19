/*V6*/

#include <v6/baker/common.h>
#include <v6/baker/baker.h>

#include <v6/core/filesystem.h>

BEGIN_V6_BAKER_NAMESPACE

CBaker::CBaker()
	: m_pFileSystem(new core::CFileSystem())
{
}

CBaker::~CBaker()
{

}

END_V6_BAKER_NAMESPACE