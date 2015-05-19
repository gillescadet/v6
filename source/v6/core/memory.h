/*V6*/

#pragma once

#ifndef __V6_CORE_MEMORY_H__
#define __V6_CORE_MEMORY_H__

BEGIN_V6_CORE_NAMESPACE

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

class IStack : public IHeap
{
public:
	virtual void	free( void* ) override {}
	virtual void *	realloc( void*, int nSize) override { return alloc( nSize ); }

	virtual void	push() = 0;
	virtual void	pop() = 0;
};

class CHeap : public IHeap
{
public:
	virtual void *	alloc(int nSize) override;
	virtual void	free(void * p) override;
	virtual void *	realloc(void * p, int nSize) override;
};

class Stack : public IStack
{
public:
					Stack( IHeap* heap, uint capacity = 1024 * 1024 );
	virtual			~Stack();

public:
	virtual void *	alloc( int size ) override;
	virtual void	push() override;
	virtual void	pop() override;

private:
	IHeap*			m_heap;
	uint*			m_stack;
	void*			m_buffer;
	uint			m_size;
	uint			m_capacity;	
	uint			m_stackSize;
};

class ScopedStack
{
public:
	ScopedStack( IStack* stack ) : m_stack(stack)
	{
		m_stack->push();
	}
	~ScopedStack()
	{
		m_stack->pop();
	}

private:
	IStack* m_stack;
};

struct Block_s;

struct BlockAllocator_s
{
	IHeap*	 heap;
	Block_s* firstBlock;
	u32		 blockCapacity;
};

void* BlockAllocator_Alloc( BlockAllocator_s* allocator, u32 size );
void BlockAllocator_Clear( BlockAllocator_s* allocator );
void BlockAllocator_Create( BlockAllocator_s* allocator, IHeap* heap, u32 blockCapacity );
void BlockAllocator_Release( BlockAllocator_s* allocator );

class CBlockAllocator : public IAllocator
{
public:
	CBlockAllocator( IHeap & oHeap, int nBlockCapacity = 4096 );
	virtual ~CBlockAllocator();

public:
	virtual void *	alloc( int nSize ) override;
	void			clear();

private:
	BlockAllocator_s allocator;
};

struct GrowingAllocator_s
{
	IHeap*	heap;
	void*	data;
	u32		size;
	u32		capacity;
};

template < typename T >
T* GrowingAllocator_Add( BlockAllocator_s* allocator, u32 count );
void GrowingAllocator_Extend( BlockAllocator_s* allocator, u32 size );
void GrowingAllocator_Create( GrowingAllocator_s* allocator, IHeap* heap );
template < typename T >
T* GrowingAllocator_Get( GrowingAllocator_s* allocator, u32 index );
void GrowingAllocator_Release( GrowingAllocator_s* allocator );

template < typename T >
T* GrowingAllocator_Add( BlockAllocator_s* allocator, u32 count )
{
	const u32 size = allocator->size;
	GrowingAllocator_Extend( allocator, count * sizeof( T ) );
	return (T*)((u8*)allocator->data + size);
}

template < typename T >
T* GrowingAllocator_Get( GrowingAllocator_s* allocator, u32 index )
{ 
	return (T*)((u8*)allocator->data + index * sizeof( T )); 
}

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_MEMORY_H__