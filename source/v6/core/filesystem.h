/*V6*/

#pragma once

#ifndef __V6_CORE_FILESYSTEM_H__
#define __V6_CORE_FILESYSTEM_H__

BEGIN_V6_NAMESPACE

class IAllocator;

typedef void (*FileCallback) ( const char *pFileName, void * pCallbackData, const char* filter );

void	FilePath_ChangeExtension( char* filePathWithNewExtension, u32 maxSize, const char* filePath, const char* extension );
void	FilePath_ExtractExtension( char* extension, u32 maxSize, const char* filePath );
void	FilePath_ExtractFilename( char* filename, u32 maxSize, const char* filePath );
void	FilePath_ExtractPath( char* path, u32 maxSize, const char* filePath );
bool	FilePath_HasExtension( const char* filePath, const char* extension );
void	FilePath_Make( char* filePath, u32 maxSize, char* path, char* filename );
void	FilePath_TrimExtension( char* filePathWithoutExtension, u32 maxSize, const char* filePath );

bool	FileSystem_CreateDirectory( const char* filename );
bool	FileSystem_DeleteFile( const char* filename );
bool	FileSystem_GetFileList( const char * pFilter, FileCallback pFileCallback, void* pCallbackData );
bool	FileSystem_GetLocalAppDataPath( char* filePath, u32 filePathMaxSize );
bool	FileSystem_FileExists( const char* filename );
int		FileSystem_ReadFile( const char* filename, void** data, IAllocator* allocator );

bool	FileDialog_Open( char* filename, u32 maxSizeOfFilename, const char* extension );

END_V6_NAMESPACE

#endif // __V6_CORE_FILESYSTEM_H__