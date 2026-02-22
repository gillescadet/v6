/*V6*/

#include <v6/core/common.h>

#include <v6/core/math.h>
#include <v6/core/memory.h>

#pragma warning( disable: 4722 ) //	destructor never returns, potential memory leak

#if V6_MEMORY_CHECK_CONTEXT == 1
#pragma message( "### SLOW MEMORY CONTEXT ENABLED ###" )
#endif

BEGIN_V6_NAMESPACE

#if V6_MEMORY_CHECK_CONTEXT == 1

struct MemoryContext_s
{
	const char* context;
	u32			allocationCount;
	u32			freeCount;
};

static const u32	s_memoryContextMaxCount = 16384;
MemoryContext_s		s_memoryContexts[s_memoryContextMaxCount] = {};
u32					s_memoryContextCount = 0;
static Mutex_s		s_memoryContextMutex;
#endif // #if V6_MEMORY_CHECK_CONTEXT == 1

static bool			s_heapCreated = false;

struct MemoryBlock_s
{
	u64				size;
	u64				capacity;
	MemoryBlock_s*	next;
};

#if V6_MEMORY_CHECK_CONTEXT == 1

static u32 MemoryContext_Find( const char* context )
{
	V6_ASSERT( s_heapCreated );

	u32 memoryContextCount = Atomic_Load( &s_memoryContextCount );

	u32 contextID;
	for ( contextID = 0; contextID < memoryContextCount; ++contextID )
	{
		if ( strcmp( s_memoryContexts[contextID].context, context ) == 0 )
			break;
	}

	if ( contextID < memoryContextCount )
		return contextID;

	Mutex_Lock( &s_memoryContextMutex );

	memoryContextCount = Atomic_Load( &s_memoryContextCount );

	for ( contextID; contextID < memoryContextCount; ++contextID )
	{
		if ( strcmp( s_memoryContexts[contextID].context, context ) == 0 )
			break;
	}

	if ( contextID == memoryContextCount )
	{
		V6_ASSERT( contextID < s_memoryContextMaxCount );
		s_memoryContexts[contextID].context = context;
		s_memoryContexts[contextID].allocationCount = 0;
		s_memoryContexts[contextID].freeCount = 0;

		V6_WRITE_BARRIER();
		++s_memoryContextCount;
	}

	Mutex_Unlock( &s_memoryContextMutex );

	return contextID;
}


#endif // #if V6_MEMORY_CHECK_CONTEXT == 1

CHeap::CHeap() : 
#if V6_MEMORY_CHECK_SIZE == 1
	m_allocatedSize( 0 ), 
	m_freedSize( 0 )
#endif
{
	V6_ASSERT( !s_heapCreated );
	s_heapCreated = true;

#if V6_MEMORY_CHECK_CONTEXT == 1
	Mutex_Create( &s_memoryContextMutex );
#endif
}

CHeap::~CHeap()
{
	V6_ASSERT( s_heapCreated );
	s_heapCreated = false;

#if V6_MEMORY_CHECK_CONTEXT == 1
	for ( u32 contextID = 0; contextID < s_memoryContextCount; ++contextID )
	{
		bool hasError = false;
		const MemoryContext_s* memoryContext = &s_memoryContexts[contextID];
		if ( memoryContext->allocationCount != memoryContext->freeCount )
		{
			V6_ERROR( "Memory context %s has %d allocations for %d frees\n", memoryContext->context, memoryContext->allocationCount, memoryContext->freeCount );
			hasError = true;
		}
		V6_ASSERT( !hasError );
	}
	
	Mutex_Release( &s_memoryContextMutex );
#endif

#if V6_MEMORY_CHECK_SIZE == 1
	V6_ASSERT( m_allocatedSize == m_freedSize );
#endif
}

void* CHeap::alloc( u64 size, const char* context )
{
	//V6_MSG( "allocation of %d bytes\n", size );

#if V6_MEMORY_CHECK_CONTEXT == 1 || V6_MEMORY_CHECK_SIZE == 1
	u32 infoSize = 0;

#if V6_MEMORY_CHECK_CONTEXT == 1
	const u32 contextID = MemoryContext_Find( context );
	MemoryContext_s* memoryContext = &s_memoryContexts[contextID];
	Atomic_Inc( &memoryContext->allocationCount );

	infoSize += 4;
#endif // #if V6_MEMORY_CHECK_CONTEXT == 1

#if V6_MEMORY_CHECK_SIZE == 1
	V6_ASSERT( size <= 0xFFFFFFFF );
	Atomic_Add( &m_allocatedSize, size );
	infoSize += 4;
#endif // #if V6_MEMORY_CHECK_SIZE == 1

	u32* data = (u32*)::malloc( size + infoSize );
#if V6_MEMORY_CHECK_CONTEXT == 1
	*data = (u32)contextID;
	++data;
#endif // #if V6_MEMORY_CHECK_CONTEXT == 1

#if V6_MEMORY_CHECK_SIZE == 1
	*data = (u32)size;
	++data;
#endif // #if V6_MEMORY_CHECK_SIZE == 1

#else // #if V6_MEMORY_CHECK_CONTEXT == 1 || V6_MEMORY_CHECK_SIZE == 1

	void* data = ::malloc( size );

#endif

	return data;
}

void CHeap::free( void * p )
{
	if ( p == nullptr )
		return;
	
	u32* data = (u32*)p;

#if V6_MEMORY_CHECK_CONTEXT == 1 || V6_MEMORY_CHECK_SIZE == 1

#if V6_MEMORY_CHECK_SIZE == 1
	--data;
	const u64 size = *data;
	Atomic_Add( &m_freedSize, size );
#endif // #if V6_MEMORY_CHECK_SIZE == 1

#if V6_MEMORY_CHECK_CONTEXT == 1
	--data;
	const u32 contextID = *data;
	MemoryContext_s* memoryContext = &s_memoryContexts[contextID];
	Atomic_Inc( &memoryContext->freeCount );
#endif // #if V6_MEMORY_CHECK_CONTEXT == 1

#endif // #if V6_MEMORY_CHECK_CONTEXT == 1 || V6_MEMORY_CHECK_SIZE == 1

	::free( data );
}

void* BlockAllocator_Alloc( BlockAllocator_s* allocator, u64 size )
{
	if ( allocator->firstBlock == nullptr || allocator->firstBlock->size + size > allocator->firstBlock->capacity )
	{
		const u64 capacity = Max( size, allocator->blockCapacity );
		MemoryBlock_s* newBlock = (MemoryBlock_s *)allocator->heap->alloc( sizeof( MemoryBlock_s ) + capacity, "BlockAllocator" );
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

void BlockAllocator_Create( BlockAllocator_s* allocator, IAllocator* heap, u64 blockCapacity )
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

//--------------------------------------------------------------------------------

Stack::Stack()
{
	m_heap = nullptr;
	m_buffer = nullptr;
	m_size = 0;
	m_capacity = 0;
	m_stackSize = 0;
}

Stack::Stack( IAllocator* heap, u64 capacity )
{
	m_heap = nullptr;
	Init( heap, capacity );
}

Stack::Stack( void* buffer, u64 capacity )
{
	m_heap = nullptr;
	Init( buffer, capacity );
}

void Stack::Release()
{
	V6_ASSERT( m_stackSize == 0 );
	if ( m_heap )
	{
		m_heap->free( m_buffer );
		m_heap = nullptr;
	}
}

Stack::~Stack()
{
	Release();
}

void Stack::Init( IAllocator* heap, u64 capacity )
{
	V6_ASSERT( m_heap == nullptr );
	m_heap = heap;
	m_buffer = m_heap->alloc( capacity, "Stack" );
	m_capacity = capacity;
	m_size = 0;
	m_stackSize = 0;
}

void Stack::Init( void* buffer, u64 capacity )
{
	V6_ASSERT( m_heap == nullptr );
	m_buffer = buffer;
	m_capacity = capacity;
	m_size = 0;
	m_stackSize = 0;
}

void * Stack::alloc( u64 size, const char* context )
{
	V6_ASSERT( m_size + size <= m_capacity );
	void* p = (u8*)m_buffer + m_size;
	m_size += size;
	return p;
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

//--------------------------------------------------------------------------------

QueueAllocator::QueueAllocator()
{
	m_heap = nullptr;
	m_buffer = nullptr;
	m_capacity = 0;
	m_sizeBegin = 0;
	m_sizeEnd = 0;
	m_queueBegin = 0;
	m_queueEnd = 0;
}

QueueAllocator::QueueAllocator( IAllocator* heap, u64 capacity )
{
	m_heap = nullptr;
	Init( heap, capacity );
}

void QueueAllocator::Release()
{
	V6_ASSERT( m_queueBegin == m_queueEnd );
	if ( m_heap )
	{
		m_heap->free( m_buffer );
		m_heap = nullptr;
		Mutex_Release( &m_mutex );
	}
}

QueueAllocator::~QueueAllocator()
{
	Release();
}

void QueueAllocator::Init( IAllocator* heap, u64 capacity )
{
	V6_ASSERT( m_heap == nullptr );
	Mutex_Create( &m_mutex );
	m_heap = heap;
	m_buffer = m_heap->alloc( capacity, "QueueAllocator" );
	m_capacity = capacity;
	m_sizeBegin = 0;
	m_sizeEnd = 0;
	m_queueBegin = 0;
	m_queueEnd = 0;
}

void* QueueAllocator::alloc( u64 size, const char* context )
{
	Mutex_Lock( &m_mutex );

	V6_ASSERT( m_sizeEnd - m_sizeBegin + size <= m_capacity );

	const u64 allocBegin = m_sizeEnd % m_capacity;
	const u64 allocEnd = (m_sizeEnd + size) % m_capacity;

	if ( allocEnd < allocBegin )
	{
		m_sizeEnd += m_capacity - allocBegin;
		Mutex_Unlock( &m_mutex );
		return alloc( size, context );
	}

	void* p = (u8*)m_buffer + allocBegin;
	m_sizeEnd += size;
	
	Mutex_Unlock( &m_mutex );

	return p;
}

void QueueAllocator::push()
{
	V6_ASSERT( m_queueEnd - m_queueBegin < LEVEL_COUNT );
	if ( m_queueBegin != m_queueEnd )
		m_queue[(m_queueEnd-1) & LEVEL_MOD] = m_sizeEnd;
	m_queue[m_queueEnd & LEVEL_MOD] = m_sizeEnd;
	++m_queueEnd;
}

void QueueAllocator::pop()
{
	V6_ASSERT( m_queueEnd - m_queueBegin > 0 );
	m_sizeBegin = m_queue[m_queueBegin & LEVEL_MOD];
	++m_queueBegin;
}

void QueueAllocator::Clear()
{
	m_sizeBegin = 0;
	m_sizeEnd = 0;
	m_queueBegin = 0;
	m_queueEnd = 0;
}

//--------------------------------------------------------------------------------

END_V6_NAMESPACE
