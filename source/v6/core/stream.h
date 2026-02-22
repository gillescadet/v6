/*V6*/

#pragma once

#ifndef __V6_CORE_STREAM_H__
#define __V6_CORE_STREAM_H__

BEGIN_V6_NAMESPACE

class IAllocator;

struct x64 { u64 v;  };

#define ToU64( X64 ) X64.v
#define ToX64( U64 ) x64 { U64 }

class IStreamWriter
{
public:
	virtual x64		GetPos() const = 0;
	virtual x64		GetSize() const = 0;
	virtual void	SetPos( x64 pos ) = 0;
	virtual void	Write( const void * pData, x64 nSize ) = 0;
	void			WriteAligned( const void * pData, x64 nSize, u32 alignment );
	void			WriteString( const char* str );
	void			WriteZero( u32 count );
	void			WriteZeroUntilAligned( u32 alignment );
};

class IStreamReader
{
public:
	virtual x64		GetPos() const = 0;
	virtual x64		GetRemaining() const { return ToX64( ToU64( GetSize() ) - ToU64( GetPos() ) ); }
	virtual x64		GetSize() const = 0;
	virtual x64		Read( x64 nSize, void * pData ) = 0;
	void			ReadAligned( x64 nSize, void * pData, u32 alignment );
	virtual void	SetPos( x64 pos ) = 0;
	virtual void	Skip( x64 nSize ) = 0;
	void			SkipUntilAligned( u32 alignment );
};

enum
{
	FILE_OPEN_FLAG_EXTEND		= 1 << 0,
	FILE_OPEN_FLAG_UNBUFFERED	= 1 << 1
};

class CFileReader : public IStreamReader
{
public:
	CFileReader();
	virtual ~CFileReader();

public:
	bool IsOpen() { return m_file != nullptr; }
	bool Open( const char* filename, u32 flags );
	void Close();
	const char* GetFilename() const { return m_filename; }

public:
	virtual x64 GetPos() const;
	virtual x64 GetSize() const;
	virtual x64 Read( x64 nSize, void *data );
	virtual void SetPos( x64 pos );
	virtual void Skip( x64 nSize );

private:
	void*	m_file;
	char	m_filename[256];
};

class CFileWriter : public IStreamWriter
{
public:
	CFileWriter();
	virtual ~CFileWriter();

public:
	bool Open( const char* filename, u32 flags );
	void Close();
	const char* GetFilename() const { return m_filename; }

public:
	virtual x64 GetPos() const;
	virtual x64 GetSize() const;
	virtual void SetPos( x64 pos );
	virtual void Write( const void * pData, x64 nSize );

private:
	void*	m_file;
	char	m_filename[256];
};

class CStreamReaderWithBuffering : public IStreamReader
{
public:
	CStreamReaderWithBuffering( IStreamReader* backendStreamReader, u8* alignedBuffer, u32 bufferSize );
	virtual ~CStreamReaderWithBuffering();

public:
	u32				GetBufferSize() const { return m_bufferSize; }
	virtual x64		GetPos() const;
	virtual x64		GetSize() const;
	virtual x64		Read( x64 nSize, void * pData );
	virtual void	SetPos( x64 pos );
	virtual void	Skip( x64 nSize );
	void			SkipUnreadBuffer();

private:
	IStreamReader*	m_backendStreamReader;
	u8*				m_alignedBuffer;
	u32				m_bufferPos;
	u32				m_bufferSize;
};

class CStreamWriterWithBuffering : public IStreamWriter
{
public:
	CStreamWriterWithBuffering( IStreamWriter* backendStreamWriter, u8* alignedBuffer, u32 bufferSize );
	virtual ~CStreamWriterWithBuffering();

public:
	void			FlushBufferAndPadWithZero();
	u32				GetBufferSize() const { return m_bufferSize; }
	virtual x64		GetPos() const;
	virtual x64		GetSize() const;
	virtual void	SetPos( x64 pos );
	virtual void	Write( const void * pData, x64 nSize );

private:
	IStreamWriter*	m_backendStreamWriter;
	u8*				m_alignedBuffer;
	u32				m_bufferPos;
	u32				m_bufferSize;
};

class CBufferReader : public IStreamReader
{
public:
	CBufferReader( const void * pBuffer, x64 nSize );

public:
	const void * GetBuffer() { return m_pBuffer; }
	virtual x64 GetPos() const { return m_nPos; }
	virtual x64 GetSize() const { return m_nSize; }
	virtual x64 Read( x64 nSize, void * pData );
	virtual void SetPos( x64 pos );
	virtual void Skip( x64 nSize );

private:
	const void*		m_pBuffer;
	x64				m_nPos;
	x64				m_nSize;
};

class CBufferWriter : public IStreamWriter
{
public:
	CBufferWriter(void * pBuffer, x64 nSize);

public:
	void * GetBuffer() { return m_pBuffer; }
	virtual x64 GetPos() const { return m_nPos; }
	virtual x64 GetSize() const { return m_nSize; }
	virtual void SetPos( x64 pos );
	virtual void Write( const void * pData, x64 nSize);

private:
	void*	m_pBuffer;
	x64		m_nPos;
	x64		m_nSize;
};

END_V6_NAMESPACE

#endif // __V6_CORE_STREAM_H__