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

/// IStreamWriter

void IStreamWriter::WriteZero( u32 count )
{
	const u8 bufferOfZeros[256] = {};

	u32 remainingCount = count;
	while ( remainingCount >= sizeof( bufferOfZeros ) )
	{
		this->Write( bufferOfZeros, ToX64( sizeof( bufferOfZeros ) ) );
		remainingCount -= sizeof( bufferOfZeros );
	}

	if ( remainingCount )
		this->Write( bufferOfZeros, ToX64( remainingCount ) );
}

void IStreamWriter::WriteZeroUntilAligned( u32 alignment )
{
	V6_ASSERT( IsPowOfTwo( alignment ) );
	const u8 bufferOfZeros[256] = {};
	V6_ASSERT( alignment <= sizeof( bufferOfZeros ) );
	const u32 offset = ToU64( this->GetPos() ) & (alignment-1);
	if ( offset == 0 )
		return;
	const u32 remaining = alignment - offset;
	this->Write( bufferOfZeros, ToX64( remaining ) );
}

void IStreamWriter::WriteAligned( const void * pData, x64 nSize, u32 alignment )
{
	V6_ASSERT( (ToU64( this->GetPos() ) & (alignment-1)) == 0 );
	this->Write( pData, nSize );
	this->WriteZeroUntilAligned( alignment );
}

void IStreamWriter::WriteString( const char* str )
{
	this->Write( str, ToX64( strlen( str ) ) );
	const u8 zero = 0;
	this->Write( &zero, ToX64( 1 ) );
}

/// IStreamReader

void IStreamReader::SkipUntilAligned( u32 alignment )
{
	V6_ASSERT( IsPowOfTwo( alignment ) );
	u8 buffer[256] = {};
	V6_ASSERT( alignment <= sizeof( buffer ) );
	const u32 offset = ToU64( this->GetPos() ) & (alignment-1);
	if ( offset == 0 )
		return;
	const u32 remaining = alignment - offset;
	this->Read( ToX64( remaining ), buffer );
}

void IStreamReader::ReadAligned( x64 nSize, void * pData, u32 alignment )
{
	V6_ASSERT( (ToU64( this->GetPos() ) & (alignment-1)) == 0 );
	this->Read( nSize, pData );
	this->SkipUntilAligned( alignment );
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

/// CStreamReaderWithBuffering

CStreamReaderWithBuffering::CStreamReaderWithBuffering( IStreamReader* backendStreamReader, u8* alignedBuffer, u32 bufferSize )
	: m_backendStreamReader( backendStreamReader )
	, m_alignedBuffer( alignedBuffer )
	, m_bufferPos ( bufferSize )
	, m_bufferSize ( bufferSize )
{
	V6_ASSERT( ((uintptr_t)alignedBuffer & ((uintptr_t)bufferSize-1)) == 0 );
}

CStreamReaderWithBuffering::~CStreamReaderWithBuffering()
{
	V6_ASSERT( m_bufferPos == m_bufferSize );
}

x64 CStreamReaderWithBuffering::GetPos() const
{
	return ToX64( ToU64( m_backendStreamReader->GetPos() ) + m_bufferPos - m_bufferSize );
}

x64 CStreamReaderWithBuffering::GetSize() const
{
	return m_backendStreamReader->GetSize();
}

void CStreamReaderWithBuffering::SetPos( x64 pos )
{
	V6_ASSERT( !"Not supported" );
}

void CStreamReaderWithBuffering::Skip( x64 nSize )
{
	V6_ASSERT( !"Not supported" );
}

void CStreamReaderWithBuffering::SkipUnreadBuffer()
{
	m_bufferPos = m_bufferSize;
}

void CStreamReaderWithBuffering::Read( x64 nSize, void * pData )
{
	u8* curData = (u8*)pData;
	u32 curSize = (u32)ToU64( nSize );

	if ( curSize == 0 )
		return;

	if ( m_bufferPos == m_bufferSize )
	{
		m_backendStreamReader->Read( ToX64( m_bufferSize ), m_alignedBuffer );
		m_bufferPos = 0;
	}

	const u32 remainingToReadBuffer = m_bufferSize - m_bufferPos;
	if ( curSize < remainingToReadBuffer )
	{
		memcpy( curData, m_alignedBuffer + m_bufferPos, curSize );
		m_bufferPos += curSize;
		return;
	}

	{
		memcpy( curData, m_alignedBuffer + m_bufferPos, remainingToReadBuffer );
		m_bufferPos = m_bufferSize;
		curData += remainingToReadBuffer;
		curSize -= remainingToReadBuffer;
	}

	const u32 remainingToExtendBuffer = curSize % m_bufferSize;
	if ( curSize > remainingToExtendBuffer )
	{
		curSize -= remainingToExtendBuffer;
		do 
		{
			m_backendStreamReader->Read( ToX64( m_bufferSize ), m_alignedBuffer );
			memcpy( curData, m_alignedBuffer, m_bufferSize );
			curData += m_bufferSize;
			curSize -= m_bufferSize;
		} while ( curSize > 0 );
	}

	if ( remainingToExtendBuffer > 0 )
	{
		m_backendStreamReader->Read( ToX64( m_bufferSize ), m_alignedBuffer );
		memcpy( curData, m_alignedBuffer, remainingToExtendBuffer );
		m_bufferPos = remainingToExtendBuffer;
	}
}

/// CStreamWriterWithBuffering

CStreamWriterWithBuffering::CStreamWriterWithBuffering( IStreamWriter* backendStreamWriter, u8* alignedBuffer, u32 bufferSize )
	: m_backendStreamWriter( backendStreamWriter )
	, m_alignedBuffer( alignedBuffer )
	, m_bufferPos ( 0 )
	, m_bufferSize ( bufferSize )
{
	V6_ASSERT( ((uintptr_t)alignedBuffer & ((uintptr_t)bufferSize-1)) == 0 );
}

CStreamWriterWithBuffering::~CStreamWriterWithBuffering()
{
	V6_ASSERT( m_bufferPos == 0 );
}

void CStreamWriterWithBuffering::FlushBufferAndPadWithZero()
{
	if ( m_bufferPos == 0 )
		return;
	
	memset( m_alignedBuffer + m_bufferPos, 0, m_bufferSize - m_bufferPos );
	m_backendStreamWriter->Write( m_alignedBuffer, ToX64( m_bufferSize ) );
	m_bufferPos = 0;
}

x64 CStreamWriterWithBuffering::GetPos() const
{
	return ToX64( ToU64( m_backendStreamWriter->GetPos() ) + m_bufferPos );
}

x64 CStreamWriterWithBuffering::GetSize() const
{
	return m_backendStreamWriter->GetSize();
}

void CStreamWriterWithBuffering::SetPos( x64 pos )
{
	V6_ASSERT( !"Not supported" );
}

void CStreamWriterWithBuffering::Write( const void * pData, x64 nSize )
{
	u8* curData = (u8*)pData;
	u32 curSize = (u32)ToU64( nSize );
	
	if ( curSize == 0 )
		return;

	const u32 remainingToFillBuffer = m_bufferSize - m_bufferPos;
	if ( curSize < remainingToFillBuffer )
	{
		memcpy( m_alignedBuffer + m_bufferPos, curData, curSize );
		m_bufferPos += curSize;
		return;
	}

	{
		memcpy( m_alignedBuffer + m_bufferPos, curData, remainingToFillBuffer );
		m_backendStreamWriter->Write( m_alignedBuffer, ToX64( m_bufferSize ) );
		m_bufferPos = 0;
		curData += remainingToFillBuffer;
		curSize -= remainingToFillBuffer;
	}

	const u32 remainingToExtendBuffer = curSize % m_bufferSize;
	if ( curSize > remainingToExtendBuffer )
	{
		curSize -= remainingToExtendBuffer;
		do
		{
			memcpy( m_alignedBuffer, curData, m_bufferSize );
			m_backendStreamWriter->Write( m_alignedBuffer, ToX64( m_bufferSize ) );
			curData += m_bufferSize;
			curSize -= m_bufferSize;
		}
		while ( curSize > 0 );
	}

	if ( remainingToExtendBuffer > 0 )
	{
		memcpy( m_alignedBuffer, curData, remainingToExtendBuffer );
		m_bufferPos = remainingToExtendBuffer;
	}
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