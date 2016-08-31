/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_NAMESPACE

V6_INLINE LARGE_INTEGER LARGE_INTEGER_MAKE( u64 v )
{
	LARGE_INTEGER li;
	li.QuadPart = v;
	return li;
}

/// CFileReader

CFileReader::CFileReader()
: m_file( nullptr )
{
}

CFileReader::~CFileReader()
{
	Close();
}

bool CFileReader::Open( const char* filename, u32 flags )
{
	if ( m_file != nullptr )
	{
		V6_ASSERT( !"File already open" );
		return false;
	}

	::DWORD dwCreationDisposition = OPEN_EXISTING;
	::DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY;

	if ( flags & FILE_OPEN_FLAG_UNBUFFERED )
		dwFlagsAndAttributes |= FILE_FLAG_NO_BUFFERING;
	
	HANDLE file = CreateFileA( filename, GENERIC_READ, FILE_SHARE_READ, nullptr, dwCreationDisposition, dwFlagsAndAttributes, nullptr );
	if ( file == INVALID_HANDLE_VALUE )
		return false;

	m_file = file;
	strcpy_s( m_filename, sizeof( m_filename ), filename );

	return true;
}

void CFileReader::Close()
{
	if ( m_file != nullptr )
	{
		CloseHandle( (HANDLE)m_file );
		m_file = nullptr;
	}
}

x64 CFileReader::GetPos() const
{
	V6_ASSERT( m_file != nullptr );
	LARGE_INTEGER zero = {};
	LARGE_INTEGER pos = {};
	SetFilePointerEx( (HANDLE)m_file, zero, &pos, FILE_CURRENT );
	return ToX64( (u64)pos.QuadPart );
}

x64 CFileReader::GetSize() const
{
	V6_ASSERT( m_file != nullptr );
	LARGE_INTEGER size = {};
	GetFileSizeEx( (HANDLE)m_file, &size );
	return ToX64( (u64)size.QuadPart );
}

void CFileReader::Read( x64 nSize, void * pData )
{
	if ( ToU64( nSize ) == 0 )
		return;

	V6_ASSERT( m_file != nullptr );
	const bool done = ReadFile( (HANDLE)m_file, pData, (u32)ToU64( nSize ), nullptr, nullptr ) != 0;
	V6_ASSERT( done );
}

void CFileReader::SetPos( x64 pos )
{
	V6_ASSERT( m_file != nullptr );
	SetFilePointerEx( (HANDLE)m_file, LARGE_INTEGER_MAKE( ToU64( pos ) ), nullptr, FILE_BEGIN );
}

void CFileReader::Skip( x64 nSize )
{
	V6_ASSERT( m_file != nullptr );
	SetFilePointerEx( (HANDLE)m_file, LARGE_INTEGER_MAKE( ToU64( nSize ) ), nullptr, FILE_CURRENT );
}

/// CFileWriter

CFileWriter::CFileWriter()
	: m_file(nullptr)
{
}

CFileWriter::~CFileWriter()
{
	Close();
}

bool CFileWriter::Open( const char * pFilename, u32 flags )
{
	if ( m_file != nullptr )
	{
		V6_ASSERT( !"File already open" );
		return false;
	}

	::DWORD dwCreationDisposition = CREATE_ALWAYS;
	::DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	
	if ( flags & FILE_OPEN_FLAG_EXTEND )
		dwCreationDisposition = OPEN_EXISTING;
	
	if ( flags & FILE_OPEN_FLAG_UNBUFFERED )
		dwFlagsAndAttributes |= FILE_FLAG_NO_BUFFERING;

	HANDLE file = CreateFileA( pFilename, GENERIC_WRITE, 0, nullptr, dwCreationDisposition, dwFlagsAndAttributes, nullptr );
	if ( file == INVALID_HANDLE_VALUE )
		return false;

	if ( flags & FILE_OPEN_FLAG_EXTEND )
		SetFilePointerEx( (HANDLE)file, LARGE_INTEGER_MAKE( 0 ), nullptr, FILE_END );

	m_file = file;
	strcpy_s( m_filename, sizeof( m_filename ), pFilename );

	return true;
}

void CFileWriter::Close()
{
	if ( m_file != nullptr )
	{
		CloseHandle( (HANDLE)m_file );
		m_file = nullptr;
	}
}

void CFileWriter::SetPos( x64 pos )
{
	V6_ASSERT( m_file != nullptr );
	SetFilePointerEx( (HANDLE)m_file, LARGE_INTEGER_MAKE( ToU64( pos ) ), nullptr, FILE_BEGIN );
}

void CFileWriter::Write( const void * pData, x64 nSize )
{
	const bool done = WriteFile( (HANDLE)m_file, pData, (u32)ToU64( nSize ), nullptr, nullptr ) != 0;
	V6_ASSERT( done );
}

x64 CFileWriter::GetPos() const
{
	V6_ASSERT( m_file != nullptr );
	LARGE_INTEGER zero = {};
	LARGE_INTEGER pos = {};
	SetFilePointerEx( (HANDLE)m_file, zero, &pos, FILE_CURRENT );
	return ToX64( (u64)pos.QuadPart );
}

x64 CFileWriter::GetSize() const
{
	V6_ASSERT( m_file != nullptr );
	LARGE_INTEGER size = {};
	GetFileSizeEx( (HANDLE)m_file, &size );
	return ToX64( (u64)size.QuadPart );
}

/// CBufferReader

CBufferReader::CBufferReader( const void * pBuffer, x64 nSize )
	: m_pBuffer(pBuffer)
	, m_nPos(ToX64(0))
	, m_nSize(nSize)
{
}

void CBufferReader::Read( x64 nSize, void * pData )
{
	if ( ToU64( m_nPos ) + ToU64( nSize ) > ToU64( m_nSize ) )
	{
		V6_ASSERT(!"Buffer overrun");
		return;
	}
	memcpy( pData, (char *)m_pBuffer + ToU64( m_nPos ), ToU64( nSize ) );
	m_nPos = ToX64( ToU64( m_nPos ) + ToU64( nSize ) );
}

void CBufferReader::SetPos( x64 pos )
{
	if ( ToU64( pos ) > ToU64( m_nPos ) )
	{
		V6_ASSERT(!"Buffer overrun");
		return;
	}
	m_nPos = pos;
}

void CBufferReader::Skip( x64 nSize )
{
	if ( ToU64( m_nPos ) + ToU64( nSize ) > ToU64( m_nSize ) )
	{
		V6_ASSERT(!"Buffer overrun");
		return;
	}
	m_nPos = ToX64( ToU64( m_nPos ) + ToU64( nSize ) );
}

/// CBufferWriter

CBufferWriter::CBufferWriter(void * pBuffer, x64 nSize)
	: m_pBuffer(pBuffer)
	, m_nPos(ToX64(0))
	, m_nSize(nSize)
{
}

void CBufferWriter::SetPos( x64 pos )
{
	if ( ToU64( m_nPos ) > ToU64( m_nSize ) )
	{
		V6_ASSERT( !"Buffer overflow" );
		return;
	}
	m_nPos = pos;
}

void CBufferWriter::Write( const void * pData, x64 nSize)
{
	if ( ToU64( m_nPos ) + ToU64( nSize ) > ToU64( m_nSize ) )
	{
		V6_ASSERT( !"Buffer overflow" );
		return;
	}
	memcpy( (char *)m_pBuffer + ToU64( m_nPos ), pData, ToU64( nSize ) );
	m_nPos = ToX64( ToU64( m_nPos ) + ToU64( nSize ) );
}

END_V6_NAMESPACE