/*V6*/

#pragma once

#ifndef __V6_CORE_IMAGE_H__
#define __V6_CORE_IMAGE_H__

BEGIN_V6_CORE_NAMESPACE

class IHeap;
class IStreamWriter;
struct SColor;

class CImage
{
public:
				CImage(IHeap & oHeap, int nWidth, int nHeight);
				~CImage();

public:
	SColor *	GetColors() { return m_pPixels; }
	int			GetHeight() const { return m_nHeight; }
	int			GetWidth() const { return m_nWidth; }
	int			GetSize() const { return m_nWidth * m_nHeight * 4; }
	void		WriteBitmap(core::IStreamWriter& oStream);
	
private:
	IHeap &		m_oHeap;
	int			m_nWidth;
	int			m_nHeight;
	SColor *	m_pPixels;
};

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_IMAGE_H__