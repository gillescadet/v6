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

struct TgaHeader_s
{
	char		idlength;
	char		colourmaptype;
	char		datatypecode;
	short int	colourmaporigin;
	short int	colourmaplength;
	char		colourmapdepth;
	short int	x_origin;
	short int	y_origin;
	short		width;
	short		height;
	char		bitsperpixel;
	char		imagedescriptor;
};

#pragma pack(pop)

END_ANONYMOUS_NAMESPACE

void Image_Create( Image_s* image, IAllocator* heap, u32 width, u32 height )
{
	image->heap = heap;
	image->width = width;
	image->height = height;
	image->pixels = heap->newArray< Color_s >( width * height );
}

void Image_Clear( Image_s* image )
{
	memset( image->pixels, 0, Image_GetSize( image ) );
}

u32	Image_GetSize( Image_s* image )
{
	return image->width * image->height * 4;
}

void Image_Release( Image_s* image )
{
	image->heap->free( image->pixels );
}

void Image_WriteBitmap( Image_s* image, IStreamWriter* stream )
{
	SBitmapFileHeader bfh;
	SBitmapHeader bh;
	memset(&bfh, 0, sizeof(bfh));
	memset(&bh, 0, sizeof(bh));

	bfh.Signature = BITMAP_SIGNATURE;
	bfh.BitsOffset = sizeof(SBitmapFileHeader) + sizeof(SBitmapHeader);
	bfh.Size = Image_GetSize( image ) + bfh.BitsOffset;

	bh.HeaderSize = sizeof(SBitmapHeader);
	bh.BitCount = 32;

	bh.Compression = 3; // BITFIELD
	bh.AlphaMask = 0xff000000;
	bh.BlueMask = 0x00ff0000;
	bh.GreenMask = 0x0000ff00;
	bh.RedMask = 0x000000ff;

	bh.Planes = 1;
	bh.Height = image->height;
	bh.Width = image->width;
	bh.SizeImage = Image_GetSize( image );
	bh.PelsPerMeterX = 3780;
	bh.PelsPerMeterY = 3780;

	stream->Write(&bfh, sizeof(SBitmapFileHeader));
	stream->Write(&bh, sizeof(SBitmapHeader));
	stream->Write( image->pixels, bh.SizeImage );
}

static void MergeBytes( Color_s *pixel, unsigned char *p, int bytes )
{
	switch (bytes)
	{
	case 1:
		pixel->r = p[0];
		pixel->g = p[0];
		pixel->b = p[0];
		pixel->a = 0;
		break;
	case 2:
		pixel->r = (p[1] & 0x7c) << 1;
		pixel->g = ((p[1] & 0x03) << 6) | ((p[0] & 0xe0) >> 2);
		pixel->b = (p[0] & 0x1f) << 3;
		pixel->a = (p[1] & 0x80);
		break;
	case 3:
		pixel->r = p[2];
		pixel->g = p[1];
		pixel->b = p[0];
		pixel->a = 0;
		break;
	case 4:
		pixel->r = p[2];
		pixel->g = p[1];
		pixel->b = p[0];
		pixel->a = p[3];
		break;
	default:
		V6_ASSERT_NOT_SUPPORTED();
	}
}

bool Image_ReadTga( Image_s* image, IStreamReader* reader, IAllocator* allocator )
{
	TgaHeader_s header;
	reader->Read( sizeof(header), &header );

	if (header.datatypecode != 2 && header.datatypecode != 3 && header.datatypecode != 10)
	{
		V6_ASSERT_ALWAYS("Can only handle image type 2, 3 and 10");
		return false;
	}

	if (header.bitsperpixel != 8 && header.bitsperpixel != 16 && header.bitsperpixel != 24 && header.bitsperpixel != 32)
	{
		V6_ASSERT_ALWAYS("Can only handle pixel depths of 8, 16, 24, and 32");
		return false;
	}

	if (header.colourmaptype != 0 && header.colourmaptype != 1)
	{
		V6_ASSERT_ALWAYS("Can only handle colour map types of 0 and 1");
		return false;
	}

	Image_Create(image, allocator, header.width, header.height);
	u32 pixelCount = header.width * header.height;
	Image_Clear(image);

	u32 skipover = 0;
	skipover += header.idlength;
	skipover += header.colourmaptype * header.colourmaplength;
	reader->Skip( skipover );

	const u32 bytesPerPixel = header.bitsperpixel / 8;
	for (u32 pixelID = 0; pixelID < pixelCount;)
	{
		// Uncompressed - RGB
		if (header.datatypecode == 2)
		{
			u8 p[5];
			reader->Read( bytesPerPixel, p );
			MergeBytes(&image->pixels[pixelID], p, bytesPerPixel);
			++pixelID;
		}
		// Uncompressed - Black and White
		else if (header.datatypecode == 3)
		{
			u8 p[5];
			reader->Read( bytesPerPixel, p );
			MergeBytes(&image->pixels[pixelID], p, bytesPerPixel);
			++pixelID;
		}
		// Compressed
		else if (header.datatypecode == 10)
		{
			u8 p[5];
			reader->Read( bytesPerPixel + 1, p );
			u32 j = p[0] & 0x7f;
			MergeBytes(&image->pixels[pixelID], &p[1], bytesPerPixel);
			++pixelID;

			// RLE chunk
			if (p[0] & 0x80)
			{
				for (u32 i = 0; i < j; ++i)
				{
					MergeBytes(&image->pixels[pixelID], &p[1], bytesPerPixel);
					++pixelID;
				}
			}
			// Normal chunk
			else
			{
				for (u32 i = 0; i < j; ++i)
				{
					reader->Read( bytesPerPixel, p );
					MergeBytes(&image->pixels[pixelID], p, bytesPerPixel);
					++pixelID;
				}
			}
		}
	}

	return true;
}

CImage::CImage(IAllocator & oHeap, int nWidth, int nHeight)
{
	heap = &oHeap;
	width = nWidth;
	height = nHeight;
	pixels = (Color_s *)heap->alloc(GetSize());
}

CImage::~CImage()
{
	heap->free( pixels );
}

void CImage::WriteBitmap( IStreamWriter& oStream )
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
	oStream.Write( pixels, bh.SizeImage );
}

END_V6_CORE_NAMESPACE