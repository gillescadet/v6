/*V6*/

#pragma once

#ifndef __V6_CORE_STREAM_H__
#define __V6_CORE_STREAM_H__

BEGIN_V6_NAMESPACE

class IAllocator;

class IStreamWriter
{
public:
	virtual int GetPos() const = 0;
	virtual int GetSize() const = 0;
	virtual void SetPos( int pos ) = 0;
	virtual void Write( const void * pData, int nSize) = 0;
};

class IStreamReader
{
public:
	virtual int GetPos() const = 0;
	virtual int GetRemaining() const { return GetSize() - GetPos(); }
	virtual int GetSize() const = 0;
	virtual void Read( int nSize, void * pData ) = 0;
	virtual void SetPos( int pos ) = 0;
	virtual void Skip( int nSize ) = 0;
};

class CFileReader : public IStreamReader
{
public:
	CFileReader();
	virtual ~CFileReader();

public:
	bool Open(const char * sFilename);
	void Close();

public:
	virtual int GetPos() const;
	virtual int GetSize() const;
	virtual void Read( int nSize, void *data );
	virtual void SetPos( int pos );
	virtual void Skip( int nSize );

private:
	void* m_file;
};

class CUnbufferedFileReader : public IStreamReader
{
public:
	CUnbufferedFileReader();
	virtual ~CUnbufferedFileReader();

public:
	bool Open(const char * sFilename);
	void Close();
	const char* GetFilename() const { return m_filename; }

public:
	virtual int GetPos() const;
	virtual int GetSize() const;
	virtual void Read( int nSize, void *data );
	virtual void SetPos( int pos );
	virtual void Skip( int nSize );

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
	bool Open( const char * sFilename, bool extend );
	void Close();

public:
	virtual int GetPos() const;
	virtual int GetSize() const;
	virtual void SetPos( int pos );
	virtual void Write( const void * pData, int nSize );

private:
	void * m_pFile;
};

class CUnbufferedFileWriter : public IStreamWriter
{
public:
	CUnbufferedFileWriter();
	virtual ~CUnbufferedFileWriter();

public:
	bool Open(const char * sFilename);
	void Close();
	const char* GetFilename() const { return m_filename; }

public:
	virtual int GetPos() const;
	virtual int GetSize() const;
	virtual void SetPos( int pos );
	virtual void Write( const void * pData, int nSize );

private:
	void *	m_file;
	char	m_filename[256];
};

class CBufferReader : public IStreamReader
{
public:
	CBufferReader( const void * pBuffer, int nSize );

public:
	const void * GetBuffer() { return m_pBuffer; }
	virtual int GetPos() const { return m_nPos; }
	virtual int GetSize() const { return m_nSize; }
	virtual void Read( int nSize, void * pData );
	virtual void SetPos( int pos );
	virtual void Skip( int nSize );

private:
	const void * m_pBuffer;
	int m_nPos;
	int m_nSize;
};

class CBufferWriter : public IStreamWriter
{
public:
	CBufferWriter(void * pBuffer, int nSize);

public:
	void * GetBuffer() { return m_pBuffer; }
	virtual int GetPos() const { return m_nPos; }
	virtual int GetSize() const { return m_nSize; }
	virtual void SetPos( int pos );
	virtual void Write( const void * pData, int nSize);

private:
	void * m_pBuffer;
	int m_nPos;
	int m_nSize;
};

END_V6_NAMESPACE

#endif // __V6_CORE_STREAM_H__