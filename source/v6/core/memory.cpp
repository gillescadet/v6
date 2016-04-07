/*V6*/

#include <v6/core/common.h>
#include <v6/core/memory.h>

#include <v6/core/math.h>

BEGIN_V6_CORE_NAMESPACE

struct Block_s
{
	u32			size;
	u32			capacity;
	Block_s*	next;
};

CHeap::CHeap() : m_notFreeCount( 0 )
{
}

CHeap::~CHeap()
{
	V6_ASSERT( m_notFreeCount == 0 );
}

void * CHeap::alloc(int nSize)
{
	++m_notFreeCount;
	return ::malloc( (u32)nSize );
}

void CHeap::free(void * p)
{
	if ( p == nullptr)
		return;
	
	--m_notFreeCount;
	::free(p);
}

void * CHeap::realloc(void * p, int nSize)
{
	++m_notFreeCount;
	return ::realloc(p, (u32)nSize);
}

void* BlockAllocator_Alloc( BlockAllocator_s* allocator, u32 size )
{
	if ( allocator->firstBlock == nullptr || allocator->firstBlock->size + size > allocator->firstBlock->capacity )
	{
		u32 const capacity = Max( size, allocator->blockCapacity );
		Block_s* newBlock = (Block_s *)allocator->heap->alloc( (int)(sizeof( Block_s ) + capacity) );
		newBlock->size = size;
		newBlock->capacity = capacity;
		newBlock->next = allocator->firstBlock;
		allocator->firstBlock = newBlock;
		return (void*)(newBlock+1);
	}

	void* data = (u8*)(allocator->firstBlock+1) + size;
	allocator->firstBlock->size += size;
	return data;
}

void BlockAllocator_Clear( BlockAllocator_s* allocator )
{
	for ( Block_s * block = allocator->firstBlock, *nextBlock = nullptr; block; block = nextBlock)
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
	memset( allocator, 0, sizeof(BlockAllocator_s) );
}

CBlockAllocator::CBlockAllocator( IAllocator & oHeap, int nBlockCapacity )
{
	BlockAllocator_Create( &allocator, &oHeap, (u32)nBlockCapacity );
}

CBlockAllocator::~CBlockAllocator()
{
	BlockAllocator_Release( &allocator );
}

void * CBlockAllocator::alloc(int nSize)
{
	return BlockAllocator_Alloc( &allocator, (u32)nSize );
}

void CBlockAllocator::clear()
{
	BlockAllocator_Clear( &allocator );
}

static const u32 STACK_CAPACITY = 32;

Stack::Stack( IAllocator* heap, u32 capacity )
	: m_heap( heap )
{
	m_buffer = m_heap->alloc( (int)capacity );
	m_capacity = capacity;
	m_size = 0;
	m_stack = (u32*)Stack::alloc( STACK_CAPACITY * sizeof(u32) );
	m_stackSize = 0;
}

Stack::~Stack()
{
	V6_ASSERT( m_stackSize == 0 );
	m_heap->free( m_buffer );
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
	V6_ASSERT( m_stackSize < STACK_CAPACITY );
	m_stack[m_stackSize++] = m_size;
}

void Stack::pop()
{
	V6_ASSERT( m_stackSize > 0 );
	m_size = m_stack[--m_stackSize];
}

void GrowingAllocator_Extend( GrowingAllocator_s* allocator, u32 size )
{
	if ( allocator->size + size > allocator->capacity )
	{
		void* data = allocator->data;
		allocator->capacity = Max( allocator->size + size, allocator->capacity * 2 );
		allocator->data = allocator->heap->alloc( (int)allocator->capacity );		
		memcpy( allocator->data, data, allocator->size );		
		allocator->heap->free( data );
	}
	allocator->size += size;
}

void GrowingAllocator_Create( GrowingAllocator_s* allocator, IAllocator* heap )
{
	allocator->heap = heap;
	allocator->data = nullptr;
	allocator->size = 0;
	allocator->capacity = 0;
}

void GrowingAllocator_Release( GrowingAllocator_s* allocator )
{
	allocator->heap->free( allocator->data );
	memset( allocator, 0, sizeof( GrowingAllocator_s ) );
}

END_V6_CORE_NAMESPACE
