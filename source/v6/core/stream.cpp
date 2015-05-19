/*V6*/

#include <v6/core/common.h>
#include <v6/core/stream.h>

#include <v6/core/math.h>
#include <v6/core/memory.h>

BEGIN_V6_CORE_NAMESPACE

CFileWriter::CFileWriter()
: m_pFile(nullptr)
, m_nPos(0)
, m_nSize(0)
{
}

CFileWriter::~CFileWriter()
{
	Close();
}

bool CFileWriter::Open(const char * pFilename)
{
	if (m_pFile != nullptr)
	{
		V6_ASSERT( !"File already open" );
		return false;
	}

	FILE * pFile = (FILE *)m_pFile;
	if (fopen_s(&pFile, pFilename, "wb") != 0)
	{
		return false;
	}

	m_pFile = pFile;

	return true;
}

void CFileWriter::Close()
{
	if (m_pFile != nullptr)
	{
		fclose((FILE*)m_pFile);
		m_nPos = 0;
		m_nSize = 0;
	}
}

void CFileWriter::Write( const void * pData, int nSize )
{
	fwrite( pData, (size_t)nSize, 1, (FILE *)m_pFile );
	m_nPos += nSize;
	m_nSize += nSize;
}

CMemoryWriter::CMemoryWriter(IHeap* oHeap)
: m_oHeap(oHeap)
, m_pBuffer(nullptr)
, m_nPos(0)
, m_nSize(0)
{
}

CMemoryWriter::~CMemoryWriter()
{
	Clear();
}

void CMemoryWriter::Clear()
{
	m_oHeap->free(m_pBuffer);
	m_pBuffer = nullptr;
	m_nPos = 0;
	m_nSize = 0;
}

void CMemoryWriter::Resize(int nSize)
{
	if (nSize != m_nSize)
	{
		if (nSize == 0)
		{
			Clear();
		}
		else
		{
			m_nSize = nSize;
			m_pBuffer = m_oHeap->realloc(m_pBuffer, m_nSize);
			m_nPos = Min(m_nPos, m_nSize);
		}
	}
}

void CMemoryWriter::Write( const void * pData, int nSize)
{
	if (m_nPos + nSize > m_nSize)
	{
		m_nSize += Max(m_nSize, nSize);
		m_pBuffer = m_oHeap->realloc(m_pBuffer, m_nSize);
	}
	memcpy((char *)m_pBuffer + m_nPos, pData, (size_t)nSize);
	m_nPos += nSize;
}

CBufferReader::CBufferReader( const void * pBuffer, int nSize )
	: m_pBuffer(pBuffer)
	, m_nPos(0)
	, m_nSize(nSize)
{
}

void CBufferReader::Read( int nSize, void * pData )
{
	if ( m_nPos + nSize > m_nSize )
	{
		V6_ASSERT(!"Buffer overrun");
		return;
	}
	memcpy( pData, (char *)m_pBuffer + m_nPos, (size_t)nSize);
	m_nPos += nSize;
}

CBufferWriter::CBufferWriter(void * pBuffer, int nSize)
	: m_pBuffer(pBuffer)
	, m_nPos(0)
	, m_nSize(nSize)
{
}

void CBufferWriter::Write( const void * pData, int nSize)
{
	if (m_nPos + nSize > m_nSize)
	{
		V6_ASSERT(!"Buffer overflow");
		return;
	}
	memcpy((char *)m_pBuffer + m_nPos, pData, (size_t)nSize);
	m_nPos += nSize;
}

END_V6_CORE_NAMESPACE