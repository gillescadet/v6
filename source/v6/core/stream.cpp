/*V6*/

#include <v6/core/common.h>

#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_NAMESPACE

/// CFileReader

CFileReader::CFileReader()
: m_file( nullptr )
{
}

CFileReader::~CFileReader()
{
	Close();
}

bool CFileReader::Open( const char* filename )
{
	if ( m_file != nullptr )
	{
		V6_ASSERT( !"File already open" );
		return false;
	}

	FILE * file = nullptr;
	if ( fopen_s( &file, filename, "rb") != 0 )
		return false;

	m_file = file;

	return true;
}

void CFileReader::Close()
{
	if ( m_file != nullptr )
	{
		fclose( (FILE*)m_file );
		m_file = nullptr;
	}
}

int CFileReader::GetPos() const
{
	V6_ASSERT( m_file != nullptr );
	return ftell( (FILE*)m_file );
}

int CFileReader::GetSize() const
{
	V6_ASSERT( m_file != nullptr );
	const long cur = ftell( (FILE*)m_file );
	fseek( (FILE*)m_file, 0, SEEK_END );
	const long size = ftell( (FILE*)m_file );
	fseek( (FILE*)m_file, cur, SEEK_SET );
	return (int)size;
}

void CFileReader::Read( int nSize, void * pData )
{
	if ( nSize == 0 )
		return;

	V6_ASSERT( m_file != nullptr );
	const u32 elementCount = (u32)fread( pData, nSize, 1, (FILE*)m_file );
	V6_ASSERT( elementCount == 1 );
}

void CFileReader::SetPos( int pos )
{
	V6_ASSERT( m_file != nullptr );
	fseek( (FILE*)m_file, pos, SEEK_SET );
}

void CFileReader::Skip( int nSize )
{
	V6_ASSERT( m_file != nullptr );
	fseek( (FILE*)m_file, nSize, SEEK_CUR );
}

/// CFileWriter

CFileWriter::CFileWriter()
: m_pFile(nullptr)
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
		fclose((FILE*)m_pFile);
}

void CFileWriter::SetPos( int pos )
{
	V6_ASSERT( m_pFile != nullptr );
	fseek( (FILE*)m_pFile, pos, SEEK_SET );
}

void CFileWriter::Write( const void * pData, int nSize )
{
	fwrite( pData, (size_t)nSize, 1, (FILE *)m_pFile );
}

int CFileWriter::GetPos() const
{
	V6_ASSERT( m_pFile != nullptr );
	return ftell( (FILE*)m_pFile );
}

int CFileWriter::GetSize() const
{
	V6_ASSERT( m_pFile != nullptr );
	const long cur = ftell( (FILE*)m_pFile );
	fseek( (FILE*)m_pFile, 0, SEEK_END );
	const long size = ftell( (FILE*)m_pFile );
	fseek( (FILE*)m_pFile, cur, SEEK_SET );
	return (int)size;
}

/// CBufferReader

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

void CBufferReader::SetPos( int pos )
{
	if ( pos > m_nPos )
	{
		V6_ASSERT(!"Buffer overrun");
		return;
	}
	m_nPos = pos;
}

void CBufferReader::Skip( int nSize )
{
	if ( m_nPos + nSize > m_nSize )
	{
		V6_ASSERT(!"Buffer overrun");
		return;
	}
	m_nPos += nSize;
}

/// CBufferWriter

CBufferWriter::CBufferWriter(void * pBuffer, int nSize)
	: m_pBuffer(pBuffer)
	, m_nPos(0)
	, m_nSize(nSize)
{
}

void CBufferWriter::SetPos( int pos )
{
	if ( pos > m_nSize )
	{
		V6_ASSERT( !"Buffer overflow" );
		return;
	}
	m_nPos = pos;
}

void CBufferWriter::Write( const void * pData, int nSize)
{
	if ( m_nPos + nSize > m_nSize )
	{
		V6_ASSERT( !"Buffer overflow" );
		return;
	}
	memcpy( (char *)m_pBuffer + m_nPos, pData, (size_t)nSize );
	m_nPos += nSize;
}

END_V6_NAMESPACE