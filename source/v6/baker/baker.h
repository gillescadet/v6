/*V6*/

#pragma once

#ifndef __V6_BAKER_BAKER_H__
#define __V6_BAKER_BAKER_H__

BEGIN_V6_CORE_NAMESPACE

class CFileSystem;

END_V6_CORE_NAMESPACE

BEGIN_V6_BAKER_NAMESPACE

// CBaker
class CBaker
{
public:
	CBaker();
	~CBaker();

public:
	core::CFileSystem const & GetFileSystem() { return *m_pFileSystem; }

private:
	core::CFileSystem * m_pFileSystem;
};

END_V6_BAKER_NAMESPACE

#endif // __V6_BAKER_BAKER_H__