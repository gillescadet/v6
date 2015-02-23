/*V6*/

#include <v6/core/common.h>
#include <v6/core/filesystem.h>

#include <v6/core/memory.h>

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

int CFileSystem::ReadFile( const char* filename, void** data, core::IAllocator* allocator )
{
	FILE * file = NULL;

	if ( fopen_s( &file, filename, "rb" ) != 0 )
	{
		V6_ERROR( "Unable to open file %s", filename );
		return -1;
	}

	fseek( file, 0, SEEK_END );
	const core::u32 size = ftell( file );
	fseek( file, 0, SEEK_SET );

	if ( size == 0 )
	{
		fclose( file );
		*data = nullptr;
		return 0;
	}

	*data = allocator->alloc( size );
	fread( *data, size, 1, file );
	fclose( file );
	
	return size;
}

END_V6_CORE_NAMESPACE