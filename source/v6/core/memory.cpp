/*V6*/

#include <v6/core/common.h>

#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/thread.h>

#pragma warning( disable: 4722 ) //	destructor never returns, potential memory leak

BEGIN_V6_NAMESPACE

struct MemoryBlock_s
{
	u32			size;
	u32			capacity;
	MemoryBlock_s*	next;
};

CHeap::CHeap() : m_allocCount( 0 ), m_freeCount( 0 )
{
}

CHeap::~CHeap()
{
	V6_ASSERT( m_allocCount == m_freeCount );
}

void* CHeap::alloc(int nSize)
{
	// V6_MSG( "allocation of %d bytes\n", nSize );
	Atomic_Inc( &m_allocCount );
	return ::malloc( (u32)nSize );
}

void CHeap::free(void * p)
{
	if ( p == nullptr )
		return;
	
	Atomic_Inc( &m_freeCount );
	::free(p);
}

void * CHeap::realloc(void * p, int nSize)
{
	Atomic_Inc( &m_allocCount );
	return ::realloc(p, (u32)nSize);
}

void* BlockAllocator_Alloc( BlockAllocator_s* allocator, u32 size )
{
	if ( allocator->firstBlock == nullptr || allocator->firstBlock->size + size > allocator->firstBlock->capacity )
	{
		const u32 capacity = Max( size, allocator->blockCapacity );
		MemoryBlock_s* newBlock = (MemoryBlock_s *)allocator->heap->alloc( (int)(sizeof( MemoryBlock_s ) + capacity) );
		newBlock->size = size;
		newBlock->capacity = capacity;
		newBlock->next = allocator->firstBlock;
		allocator->firstBlock = newBlock;
		return (void*)(newBlock+1);
	}

	void* data = (u8*)(allocator->firstBlock+1) + allocator->firstBlock->size;
	allocator->firstBlock->size += size;
	return data;
}

void BlockAllocator_Clear( BlockAllocator_s* allocator )
{
	for ( MemoryBlock_s* block = allocator->firstBlock, *nextBlock = nullptr; block; block = nextBlock )
	{
		nextBlock = block->next;
		allocator->heap->free( block );
	}
	allocator->firstBlock = nullptr;
}

void BlockAllocator_Create( BlockAllocator_s* allocator, IAllocator* heap, u32 blockCapacity )
{
	allocator->heap = heap;
	allocator->firstBlock = nullptr;
	allocator->blockCapacity = blockCapacity;
}

void BlockAllocator_Release( BlockAllocator_s* allocator )
{
	BlockAllocator_Clear( allocator );
	memset( allocator, 0, sizeof( BlockAllocator_s ) );
}

Stack::Stack()
{
	m_heap = nullptr;
	m_buffer = nullptr;
	m_size = 0;
	m_capacity = 0;
	m_stackSize = 0;
}

Stack::Stack( IAllocator* heap, u32 capacity )
{
	Init( heap, capacity );
}

Stack::~Stack()
{
	V6_ASSERT( m_stackSize == 0 );
	if ( m_heap )
		m_heap->free( m_buffer );
}

void Stack::Init( IAllocator* heap, u32 capacity )
{
	m_heap = heap;
	m_buffer = m_heap->alloc( (int)capacity );
	m_capacity = capacity;
	m_size = 0;
	m_stackSize = 0;
}

void * Stack::alloc( int size )
{
	V6_ASSERT( m_size + size <= m_capacity );
	void* p = (u8*)m_buffer + m_size;
	m_size += size;
	return p;
}

void * Stack::realloc( void* p, int size )
{
	V6_ASSERT( p > m_buffer );
	const u32 prevSize = (u32)((u8*)p - (u8*)m_buffer);
	V6_ASSERT( prevSize <= m_capacity );
	V6_ASSERT( m_stackSize ? (prevSize >= m_stack[m_stackSize-1]) : true );
	m_size = prevSize; 
	return alloc( size );
}

void Stack::push()
{
	V6_ASSERT( m_stackSize < LEVEL_COUNT );
	m_stack[m_stackSize++] = m_size;
}

void Stack::pop()
{
	V6_ASSERT( m_stackSize > 0 );
	m_size = m_stack[--m_stackSize];
}

END_V6_NAMESPACE
