/*V6*/

#include <v6/core/common.h>
#include <v6/core/memory.h>

#include <v6/core/math.h>

#include <malloc.h>

BEGIN_V6_CORE_NAMESPACE

void * CHeap::alloc(int nSize)
{
	return ::malloc(nSize);
}

void CHeap::free(void * p)
{
	return ::free(p);
}

void * CHeap::realloc(void * p, int nSize)
{
	return ::realloc(p, nSize);
}

struct CBlockAllocator::SBlock
{
	int			m_nSize;
	int			m_nCapacity;
	SBlock *	m_pNext;

	void *		GetData(int nOffset) { return (void *)((char *)(this + 1) + nOffset); }
};

CBlockAllocator::CBlockAllocator(IHeap & oHeap, int nBlockCapacity)
	: m_oHeap(oHeap)
	, m_pFirstBlock(nullptr)
	, m_nBlockCapacity(nBlockCapacity)
{
}

CBlockAllocator::~CBlockAllocator()
{
	clear();
}

void * CBlockAllocator::alloc(int nSize)
{
	if (m_pFirstBlock == nullptr || m_pFirstBlock->m_nSize + nSize > m_pFirstBlock->m_nCapacity)
	{
		int const nCapacity = Max(nSize, m_nBlockCapacity);
		SBlock * pNewBlock = (SBlock *)m_oHeap.alloc(sizeof(SBlock) + nCapacity);
		pNewBlock->m_nSize = nSize;
		pNewBlock->m_nCapacity = nCapacity;
		pNewBlock->m_pNext = m_pFirstBlock;
		m_pFirstBlock = pNewBlock;
		return pNewBlock->GetData(0);
	}

	void * pData = m_pFirstBlock->GetData(m_pFirstBlock->m_nSize);
	m_pFirstBlock->m_nSize += nSize;
	return pData;
}

void CBlockAllocator::clear()
{
	for (SBlock * pBlock = m_pFirstBlock, *pNextBlock = nullptr; pBlock; pBlock = pNextBlock)
	{
		pNextBlock = pBlock->m_pNext;
		m_oHeap.free(pBlock);
	}
	m_pFirstBlock = nullptr;
}

static const uint STACK_CAPACITY = 32;

Stack::Stack( IHeap* heap, uint capacity )
	: m_heap( heap )
{
	m_buffer = m_heap->alloc( capacity );
	m_capacity = capacity;
	m_size = 0;
	m_stack = (uint*)Stack::alloc( STACK_CAPACITY * sizeof(uint) );
	m_stackSize = 0;
}

Stack::~Stack()
{
	m_heap->free( m_buffer );
}

void * Stack::alloc( int size )
{
	V6_ASSERT( m_size + size <= m_capacity );
	void* p = (u8*)m_buffer + m_size;
	m_size += size;
	return p;
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


END_V6_CORE_NAMESPACE