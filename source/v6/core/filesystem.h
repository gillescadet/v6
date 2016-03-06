/*V6*/

#pragma once

#ifndef __V6_CORE_FILESYSTEM_H__
#define __V6_CORE_FILESYSTEM_H__

BEGIN_V6_CORE_NAMESPACE

class IAllocator;

void FilePath_ChangeExtension( char* filePathWithNewExtension, u32 maxSize, const char* filePath, const char* extension );
void FilePath_ExtractExtension( char* extension, u32 maxSize, const char* filePath );
void FilePath_ExtractFilename( char* filename, u32 maxSize, const char* filePath );
void FilePath_ExtractPath( char* path, u32 maxSize, const char* filePath );
bool FilePath_HasExtension( const char* filePath, const char* extension );
void FilePath_Make( char* filePath, u32 maxSize, char* path, char* filename );
void FilePath_TrimExtension( char* filePathWithoutExtension, u32 maxSize, const char* filePath );

class CFileSystem
{
public:
	typedef void(*FileCallback) (const char *pFileName, void * pCallbackData);

public:
			CFileSystem();
			~CFileSystem();

public:
	bool	GetFileList(const char * pFilter, FileCallback pFileCallback, void * pCallbackData) const;
	int		ReadFile( const char* filename, void** data, core::IAllocator* allocator );
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_FILESYSTEM_H__