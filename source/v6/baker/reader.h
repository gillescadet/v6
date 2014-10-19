/*V6*/

#pragma once

#ifndef __V6_BAKER_READER_H__
#define __V6_BAKER_READER_H__

BEGIN_V6_BAKER_NAMESPACE

class CBaker;

// CReader
class CReader
{
public:
	CReader(CBaker & oBaker);
	~CReader();

public:
	void addFile(const char * pFilename);
	void addFiles(const char * pDirectory);

private:
	CBaker & m_oBaker;
};

END_V6_BAKER_NAMESPACE

#endif // __V6_BAKER_READER_H__