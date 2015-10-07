/*V6*/

#pragma once

#ifndef __V6_BAKER_BAKER_H__
#define __V6_BAKER_BAKER_H__

BEGIN_V6_CORE_NAMESPACE

class CFileSystem;
class IAllocator;

END_V6_CORE_NAMESPACE

BEGIN_V6_BAKER_NAMESPACE

// CBaker
class CBaker
{
public:
	CBaker(core::IAllocator & oHeap);
	~CBaker();

public:
	core::CFileSystem const &	GetFileSystem() const { return *m_pFileSystem; }
	core::IAllocator &				GetHeap() { return m_oHeap; }

private:
	core::IAllocator &				m_oHeap;
	core::CFileSystem *			m_pFileSystem;	
};

END_V6_BAKER_NAMESPACE

#endif // __V6_BAKER_BAKER_H__