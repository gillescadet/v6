/*V6*/

#pragma once

#ifndef __V6_BAKER_TILEREADER_H__
#define __V6_BAKER_TILEREADER_H__

BEGIN_V6_CORE_NAMESPACE

class CBlockAllocator;
class CImage;
struct FrameBuffer;

END_V6_CORE_NAMESPACE

BEGIN_V6_BAKER_NAMESPACE

class CBaker;
struct STile;

// CTileReader
class CTileReader
{
public:
	CTileReader(CBaker & oBaker);
	~CTileReader();

public:
	bool						AddFile( const char * pFilename );
	bool						AddFiles( const char * pDirectory );
	void						FillFrameBuffer( core::FrameBuffer* frameBuffer );
	void						FillColorImage(core::CImage & oImage);
	void						FillDepthImage(core::CImage & oImage, float fMinDepth, float fMaxDepth);	
	void						GetDepthRange(float & fMin, float & fMax) const { fMin = m_fMinDepth; fMax = m_fMaxDepth; }
	void						GetImageSize(int & nWidth, int & nHeight) const { nWidth = m_nWidth; nHeight = m_nHeight; }	

private:
	CBaker &					m_oBaker;
	core::CBlockAllocator *		m_pBlockAllocator;
	STile *						m_pFirstTile;
	int							m_nWidth;
	int							m_nHeight;
	float						m_fMinDepth;
	float						m_fMaxDepth;
};

END_V6_BAKER_NAMESPACE

#endif // __V6_BAKER_TILEREADER_H__