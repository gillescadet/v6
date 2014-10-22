/*V6*/

#include <v6/core/common.h>
#include <v6/core/image.h>

#include <v6/core/color.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_CORE_NAMESPACE

BEGIN_ANONYMOUS_NAMESPACE

#ifndef __LITTLE_ENDIAN__
#	ifndef __BIG_ENDIAN__
#		define __LITTLE_ENDIAN__
#	endif
#endif

#ifdef __LITTLE_ENDIAN__
#	define BITMAP_SIGNATURE 0x4d42
#else
#	define BITMAP_SIGNATURE 0x424d
#endif

#pragma pack(push, 1)

struct SBitmapFileHeader
{
	u16 Signature;
	u32 Size;
	u32 Reserved;
	u32 BitsOffset;
};

struct SBitmapHeader
{
	u32 HeaderSize;
	s32 Width;
	s32 Height;
	u16 Planes;
	u16 BitCount;
	u32 Compression;
	u32 SizeImage;
	s32 PelsPerMeterX;
	s32 PelsPerMeterY;
	u32 ClrUsed;
	u32 ClrImportant;
	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
	u32 AlphaMask;
	u32 CsType;
	u32 Endpoints[9]; // see http://msdn2.microsoft.com/en-us/library/ms536569.aspx
	u32 GammaRed;
	u32 GammaGreen;
	u32 GammaBlue;
};

#pragma pack(pop)

END_ANONYMOUS_NAMESPACE

CImage::CImage(IHeap & oHeap, int nWidth, int nHeight)
: m_oHeap(oHeap)
, m_nWidth(nWidth)
, m_nHeight(nHeight)
{
	m_pPixels = (SColor *)m_oHeap.alloc(GetSize());
}

CImage::~CImage()
{
	m_oHeap.free(m_pPixels);
}

void CImage::WriteBitmap(core::IStreamWriter& oStream)
{
	SBitmapFileHeader bfh;
	SBitmapHeader bh;
	memset(&bfh, 0, sizeof(bfh));
	memset(&bh, 0, sizeof(bh));

	bfh.Signature = BITMAP_SIGNATURE;
	bfh.BitsOffset = sizeof(SBitmapFileHeader) + sizeof(SBitmapHeader);
	bfh.Size = GetSize() + bfh.BitsOffset;

	bh.HeaderSize = sizeof(SBitmapHeader);
	bh.BitCount = 32;

	bh.Compression = 3; // BITFIELD
	bh.AlphaMask = 0xff000000;
	bh.BlueMask = 0x00ff0000;
	bh.GreenMask = 0x0000ff00;
	bh.RedMask = 0x000000ff;

	bh.Planes = 1;
	bh.Height = GetHeight();
	bh.Width = GetWidth();
	bh.SizeImage = GetSize();
	bh.PelsPerMeterX = 3780;
	bh.PelsPerMeterY = 3780;

	oStream.Write(&bfh, sizeof(SBitmapFileHeader));
	oStream.Write(&bh, sizeof(SBitmapHeader));
	oStream.Write(m_pPixels, bh.SizeImage);
}

END_V6_CORE_NAMESPACE