/*V6*/

#pragma once

#ifndef __V6_CORE_FILESYSTEM_H__
#define __V6_CORE_FILESYSTEM_H__

BEGIN_V6_CORE_NAMESPACE

class CFileSystem
{
public:
	typedef void(*FileCallback) (const char *pFileName, void * pCallbackData);

public:
			CFileSystem();
			~CFileSystem();

public:
	bool	GetFileList(const char * pFilter, FileCallback pFileCallback, void * pCallbackData) const;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_FILESYSTEM_H__