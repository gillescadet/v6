/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#pragma warning( pop )

#include <v6/core/common.h>
#include <v6/core/filesystem.h>

#include <v6/core/memory.h>

BEGIN_V6_CORE_NAMESPACE

void FilePath_ChangeExtension( char* filePathWithNewExtension, u32 maxSize, const char* filePath, const char* extension )
{
	FilePath_TrimExtension( filePathWithNewExtension, maxSize, filePath );
	sprintf_s( filePathWithNewExtension, maxSize - strlen( filePathWithNewExtension ), "%s.%s", filePathWithNewExtension, extension );
}

void FilePath_TrimExtension( char* filePathWithoutExtension, u32 maxSize, const char* filePath )
{
	V6_ASSERT( filePathWithoutExtension ) ;
	V6_ASSERT( maxSize > 0 ) ;
	V6_ASSERT( filePath ) ;

	const char *c = filePath;
	const char* lastDot = nullptr; 
	while ( *c )
	{
		if ( *c == '.' )
			lastDot = c;
		++c;
	}

	if ( !lastDot )
	{
		strcpy_s( filePathWithoutExtension, maxSize, filePath );
		return;
	}

	const u32 count = (u32)(lastDot - filePath);
	strncpy_s( filePathWithoutExtension, maxSize, filePath, count );
}

void FilePath_ExtractExtension( char* extension, u32 maxSize, const char* filePath )
{
	V6_ASSERT( extension ) ;
	V6_ASSERT( maxSize > 0 ) ;
	V6_ASSERT( filePath ) ;
	
	const char *c = filePath;
	const char* lastDot = nullptr; 
	while ( *c )
	{
		if ( *c == '.' )
			lastDot = c;
		++c;
	}

	if ( !lastDot )
	{
		extension[0] = 0;
		return;
	}
	
	strcpy_s( extension, maxSize, lastDot+1 );
}

bool FilePath_HasExtension( const char* filePath, const char* extension )
{
	char extensionFound[256];
	FilePath_ExtractExtension( extensionFound, sizeof( extension ), filePath );
	return _stricmp( extensionFound, extension ) == 0;
}

void FilePath_ExtractPath( char* path, u32 maxSize, const char* filePath )
{
	V6_ASSERT( path ) ;
	V6_ASSERT( maxSize > 0 ) ;
	V6_ASSERT( filePath ) ;

	const char *c = filePath;
	const char* lastSeparator = nullptr; 
	while ( *c )
	{
		if ( *c == '/' || *c == '\\' )
			lastSeparator = c;
		++c;
	}

	if ( !lastSeparator )
	{
		path[0] = 0;
		return;
	}

	while ( *lastSeparator == '/' || *lastSeparator == '\\' )
		--lastSeparator;

	const u32 count = (u32)(lastSeparator - filePath + 1);
	strncpy_s( path, maxSize, filePath, count );
}

void FilePath_ExtractFilename( char* filename, u32 maxSize, const char* filePath )
{
	V6_ASSERT( filename ) ;
	V6_ASSERT( maxSize > 0 ) ;
	V6_ASSERT( filePath ) ;

	const char *c = filePath;
	const char* lastSeparator = filename; 
	while ( *c )
	{
		if ( *c == '/' || *c == '\\' )
			lastSeparator = c;
		++c;
	}

	if ( !lastSeparator )
	{
		strcpy_s( filename, maxSize, filePath );
		return;
	}
	
	strcpy_s( filename, maxSize, lastSeparator+1 );
}

void FilePath_Make( char* filePath, u32 maxSize, char* path, char* filename )
{
	V6_ASSERT( filePath ) ;
	V6_ASSERT( maxSize > 0 ) ;
	V6_ASSERT( path ) ;
	V6_ASSERT( filename ) ;

	strcpy_s( filePath, maxSize, path );

	u32 pathLen = (u32)strlen( filePath );
	if ( pathLen )
	{
		char *c = filePath + pathLen - 1;
		while ( *c == '/' || *c == '\\' ) 
		{
			--c;
			--pathLen;
		}
		filePath[pathLen] = 0;	
	
		V6_ASSERT( pathLen < maxSize );
		filePath[pathLen] = '/';
		++pathLen;
		filePath[pathLen] = 0;
	}

	strcat_s( filePath, maxSize - pathLen, filename );
}

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