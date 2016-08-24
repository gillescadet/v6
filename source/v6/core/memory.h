/*V6*/

#pragma once

#ifndef __V6_CORE_MEMORY_H__
#define __V6_CORE_MEMORY_H__

BEGIN_V6_NAMESPACE

class IAllocator
{
public:
	virtual void *	alloc(int nSize) = 0;

	template <typename T>
	T *				newArray( u32 count )
	{
		return (T*)alloc( sizeof(T) * count );
	}

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

	char*			copyString( const char* str )
	{
		const u32 l = (u32)strlen( str );
		char* p = (char*)alloc( l + 1 );
		for ( u32 c = 0; c < l; ++c )
			p[c] = str[c];
		p[l] = 0;
		return p;
	}

	template <typename T>
	void			deleteArray( T * p )
	{
		free( p );
	}

	template <typename T>
	void			deleteInstance(T * p)
	{
		p->~T();
		free(p);
	}

	virtual void	free(void * p) {};

	virtual void *	realloc(void * p, int nSize) { return nullptr; };
};

class IStack : public IAllocator
{
public:
	virtual void	free( void* ) override {}

	virtual void	push() = 0;
	virtual void	pop() = 0;
};

class CHeap : public IAllocator
{
public:
					CHeap();
	virtual			~CHeap();

public:
	virtual void *	alloc(int nSize) override;
	virtual void	free(void * p) override;
	virtual void *	realloc(void * p, int nSize) override;

private:
	u32				m_allocCount;
	u32				m_freeCount;
};

class Stack : public IStack
{
public:
	static const u32 LEVEL_COUNT = 32;

public:
					Stack();
					Stack( IAllocator* heap, u32 capacity = 1024 * 1024 );
	virtual			~Stack();

public:
	void			Init( IAllocator* heap, u32 capacity = 1024 * 1024 );

public:
	virtual void *	alloc( int size ) override;
	virtual void *	realloc( void*, int nSize) override;
	virtual void	push() override;
	virtual void	pop() override;

public:
	u32			m_stack[LEVEL_COUNT];
	IAllocator*	m_heap;
	void*		m_buffer;
	u32			m_size;
	u32			m_capacity;
	u32			m_stackSize;
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

struct BlockAllocator_s
{
	IAllocator*				heap;
	struct MemoryBlock_s*	firstBlock;
	u32						blockCapacity;
};

template < typename T > T*	BlockAllocator_Add( BlockAllocator_s* allocator, u32 count );
void*						BlockAllocator_Alloc( BlockAllocator_s* allocator, u32 size );
void						BlockAllocator_Clear( BlockAllocator_s* allocator );
void						BlockAllocator_Create( BlockAllocator_s* allocator, IAllocator* heap, u32 blockCapacity );
void						BlockAllocator_Release( BlockAllocator_s* allocator );

template < typename T >
T* BlockAllocator_Add( BlockAllocator_s* allocator, u32 count )
{
	return (T*)BlockAllocator_Alloc( allocator, count * sizeof( T ) );
}

END_V6_NAMESPACE

#endif // __V6_CORE_MEMORY_H__