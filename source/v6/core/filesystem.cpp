/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <v6/core/windows_end.h>

#include <v6/core/filesystem.h>

#include <v6/core/memory.h>

#pragma comment( lib, "shlwapi.lib" )

BEGIN_V6_NAMESPACE

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

bool FileSystem_GetFileList( const char* pFilter, FileCallback pFileCallback, void * pCallbackData )
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
			pFileCallback( oFindData.cFileName, pCallbackData, pFilter );
		}
	} while (FindNextFileA(hFind, &oFindData) != 0);
	
	FindClose(hFind);

	return true;
}

int FileSystem_ReadFile( const char* filename, void** data, IAllocator* allocator )
{
	FILE * file = NULL;

	if ( fopen_s( &file, filename, "rb" ) != 0 )
	{
		V6_ERROR( "Unable to open file %s\n", filename );
		return -1;
	}

	fseek( file, 0, SEEK_END );
	const u32 size = ftell( file );
	fseek( file, 0, SEEK_SET );

	if ( size == 0 )
	{
		fclose( file );
		*data = nullptr;
		return 0;
	}

	*data = allocator->alloc( size, "FileSystem" );
	fread( *data, size, 1, file );
	fclose( file );
	
	return size;
}

bool FileSystem_DeleteFile( const char* filename )
{
	return DeleteFileA( filename ) != 0;
}

bool FileSystem_FileExists( const char* filename )
{
	return PathFileExistsA( filename ) != 0;
}

bool FileSystem_CreateDirectory( const char* filename )
{
	const int res = SHCreateDirectoryExA( nullptr, filename, nullptr );

	return res == ERROR_SUCCESS || res == ERROR_ALREADY_EXISTS;
}

bool FileSystem_GetLocalAppDataPath( char* filePath, u32 filePathMaxSize )
{
	V6_ASSERT( filePath );
	V6_ASSERT( filePathMaxSize > 0 );
	
	PWSTR output = nullptr;
	const HRESULT hResult = SHGetKnownFolderPath( FOLDERID_LocalAppData, 0, nullptr, &output );
	
	filePath[0] = 0;

	if ( hResult == S_FALSE )
	{
		CoTaskMemFree( output );
		return false;
	}

	WideCharToMultiByte( CP_ACP, 0, output, -1, filePath, filePathMaxSize, nullptr, nullptr );

	CoTaskMemFree( output );

	return filePath[0] != 0;
}

bool FileDialog_Open( char* filename, u32 maxSizeOfFilename, const char* extension )
{
#if V6_UE4_PLUGIN == 0
	char filter[256] = {};
	sprintf_s( filter, "%s\0*.%s\0", extension );

	char title[256] = {};
	sprintf_s( title, "Open a %s file", extension );

	OPENFILENAMEA openFileName = {};
	openFileName.lStructSize = sizeof ( OPENFILENAME );
	openFileName.lpstrFile = filename;
	openFileName.nMaxFile = maxSizeOfFilename;
	openFileName.lpstrFilter = filter;
	openFileName.nFilterIndex = 1;
	openFileName.lpstrFileTitle = title;
	openFileName.nMaxFileTitle = 0;
	openFileName.lpstrInitialDir = nullptr;
	openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	
	return GetOpenFileNameA( &openFileName ) != 0;
#else
	return false;
#endif
}

END_V6_NAMESPACE