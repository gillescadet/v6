/*V6*/

#pragma once

#ifndef __V6_CORE_MEMORY_H__
#define __V6_CORE_MEMORY_H__

#include <new>

BEGIN_V6_CORE_NAMESPACE

struct SBlock;

class IAllocator
{
public:
	virtual void *	alloc(int nSize) = 0;

	template <typename T>
	T *				newInstance()
	{
		void * p = alloc(sizeof(T));
		T * t = new (p)T();
		return t;
	}

	template <typename T, typename TARG1>
	T *				newInstance(TARG1 & arg1)
	{
		void * p = alloc(sizeof(T));
		T * t = new (p)T(arg1);
		return t;
	}
};

class IHeap : public IAllocator
{
public:
	template <typename T>
	void			deleteInstance(T * p)
	{
		p->~T();
		free(p);
	}

	virtual void	free(void * p) = 0;	

	virtual void *	realloc(void * p, int nSize) = 0;
};

class CHeap : public IHeap
{
public:
	virtual void *	alloc(int nSize);
	virtual void	free(void * p);
	virtual void *	realloc(void * p, int nSize);
};

class CBlockAllocator : public IAllocator
{
public:
	CBlockAllocator(IHeap & oHeap, int nBlockCapacity = 4096);
	virtual ~CBlockAllocator();

public:
	virtual void *	alloc(int nSize);
	void			clear();

private:
	struct SBlock;

private:
	IHeap &			m_oHeap;
	SBlock *		m_pFirstBlock;
	int				m_nBlockCapacity;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MEMORY_H__