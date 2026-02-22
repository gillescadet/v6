/*V6*/

#pragma once

#ifndef __V6_CORE_MEMORY_H__
#define __V6_CORE_MEMORY_H__

#include <v6/core/math.h>
#include <v6/core/thread.h>

#define V6_MEMORY_CHECK_SIZE	1
#define V6_MEMORY_CHECK_CONTEXT 0

BEGIN_V6_NAMESPACE

class IAllocator
{
public:
	template< u32 ALIGNMENT >
	void*			alloc_aligned( void** buffer, u64 size, const char* context )
	{
		const u64 allocSize = size + ALIGNMENT - 1;
	
		void* rawData = alloc( allocSize, context );
		void* alignedData = (void*)(((uintptr_t)rawData + ALIGNMENT - 1) & ~((uintptr_t)ALIGNMENT - 1));

		if ( buffer )
			*buffer = rawData;

		return alignedData;
	}

	virtual void *	alloc( u64 nSize, const char* context ) = 0;

	template <typename T>
	T *				newArray( u64 count, const char* context )
	{
		return (T*)alloc( sizeof(T) * count, context );
	}

	template <typename T>
	T *				newInstance( const char* context )
	{
		void * p = alloc( sizeof(T), context );
		T * t = new (p)T();
		return t;
	}

	template <typename T>
	T *				newInstanceAndClear( const char* context )
	{
		void * p = alloc( sizeof(T), context );
		memset( p, 0 , sizeof(T) );
		T * t = new (p)T();
		return t;
	}

	template <typename T, typename TARG1>
	T *				newInstance( TARG1 & arg1, const char* context )
	{
		void * p = alloc( sizeof(T), context );
		T * t = new (p)T(arg1);
		return t;
	}

	char*			copyString( const char* str, const char* context )
	{
		const u32 l = (u32)strlen( str );
		char* p = (char*)alloc( l + 1, context );
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

	virtual void *	realloc( void * p, u64 nSize ) { return nullptr; };
};

class IStack : public IAllocator
{
public:
	virtual void	free( void* ) override {}

	virtual void	push() = 0;
	virtual void	pop() = 0;
};

class IQueueAllocator : public IAllocator
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
	virtual void *	alloc( u64 nSize, const char* context  ) override;
	virtual void	free( void * p ) override;

public:
#if V6_MEMORY_CHECK_SIZE == 1
	u64				GetUsedSize() const { return m_allocatedSize - m_freedSize; }
#else
	u64				GetUsedSize() const { return 0; }
#endif

private:
#if V6_MEMORY_CHECK_SIZE == 1
	u64				m_allocatedSize;
	u64				m_freedSize;
#endif // #if V6_MEMORY_CHECK_SIZE == 1
};

class Stack : public IStack
{
public:
	static const u32 LEVEL_COUNT = 32;

public:
					Stack();
					Stack( IAllocator* heap, u64 capacity = 1024 * 1024 );
					Stack( void* buffer, u64 capacity );
	virtual			~Stack();

public:
	void			Init( IAllocator* heap, u64 capacity = 1024 * 1024 );
	void			Init( void* buffer, u64 capacity );
	void			Release();

public:
	virtual void *	alloc( u64 size, const char* context ) override;
	virtual void	push() override;
	virtual void	pop() override;

public:
	u64			m_stack[LEVEL_COUNT];
	IAllocator*	m_heap;
	void*		m_buffer;
	u64			m_size;
	u64			m_capacity;
	u32			m_stackSize;
};

class QueueAllocator : public IQueueAllocator
{
public:
	static const u32 LEVEL_COUNT	= 32;
	static const u32 LEVEL_MOD		= LEVEL_COUNT-1;

public:
					QueueAllocator();
					QueueAllocator( IAllocator* heap, u64 capacity = 1024 * 1024 );
	virtual			~QueueAllocator();

public:
	void			Clear();
	void			Init( IAllocator* heap, u64 capacity = 1024 * 1024 );
	u64				GetUsedSize() const { return m_sizeEnd - m_sizeBegin; }
	void			Release();

public:
	virtual void *	alloc( u64 size, const char* context ) override;
	virtual void	push() override;
	virtual void	pop() override;

public:
	Mutex_s			m_mutex;
	u64				m_queue[LEVEL_COUNT];
	IAllocator*		m_heap;
	void*			m_buffer;
	u64				m_sizeBegin;
	u64				m_sizeEnd;
	u64				m_capacity;
	u64				m_queueBegin;
	u64				m_queueEnd;
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
	u64						blockCapacity;
};

template < typename T > T*	BlockAllocator_Add( BlockAllocator_s* allocator, u32 count );
void*						BlockAllocator_Alloc( BlockAllocator_s* allocator, u64 size );
void						BlockAllocator_Clear( BlockAllocator_s* allocator );
void						BlockAllocator_Create( BlockAllocator_s* allocator, IAllocator* heap, u64 blockCapacity );
void						BlockAllocator_Release( BlockAllocator_s* allocator );

template < typename T >
T* BlockAllocator_Add( BlockAllocator_s* allocator, u32 count )
{
	return (T*)BlockAllocator_Alloc( allocator, count * sizeof( T ) );
}

END_V6_NAMESPACE

#endif // __V6_CORE_MEMORY_H__