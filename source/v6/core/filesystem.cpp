/*V6*/

#include <v6/core/common.h>
#include <v6/core/filesystem.h>

#include <windows.h>

BEGIN_V6_CORE_NAMESPACE

CFileSystem::CFileSystem()
{
}

CFileSystem::~CFileSystem()
{
}

bool CFileSystem::GetFileList(const char * pFilter, FileCallback pFileCallback, void * pCallbackData) const
{
	if (pFilter == NULL)
	{
		V6_ASSERT(pFilter != NULL);
		return false;
	}

	if (pFileCallback == NULL)
	{
		V6_ASSERT(pFileCallback != NULL);
		return false;
	}

	WIN32_FIND_DATAA oFindData;
	
	HANDLE hFind = FindFirstFileA(pFilter, &oFindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	do
	{
		if (!(oFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			pFileCallback(oFindData.cFileName, pCallbackData);
		}
	} while (FindNextFileA(hFind, &oFindData) != 0);
	
	FindClose(hFind);

	return true;
}

END_V6_CORE_NAMESPACE