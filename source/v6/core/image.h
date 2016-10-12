/*V6*/

#pragma once

#ifndef __V6_CORE_IMAGE_H__
#define __V6_CORE_IMAGE_H__

BEGIN_V6_NAMESPACE

class IAllocator;
class IStreamReader;
class IStreamWriter;
struct Color_s;
struct ImageBlockBC1_s;

struct Image_s
{
	IAllocator*			allocator;
	Color_s *			pixels;
	u32					width;
	u32					height;
};

struct ImageBC1_s
{
	IAllocator*			allocator;
	ImageBlockBC1_s*	blocks;
	u32					width;
	u32					height;
};

class CImage : Image_s
{
public:
				CImage(IAllocator & oHeap, int nWidth, int nHeight);
				~CImage();

public:
	Color_s *	GetColors() { return pixels; }
	int			GetHeight() const { return height; }
	int			GetWidth() const { return width; }
	int			GetSize() const { return width * height * 4; }
	void		WriteBitmap( IStreamWriter& oStream );
};

void	Image_Clear( Image_s* image );
void	Image_Create( Image_s* image, IAllocator* allocator, u32 width, u32 height );
void	Image_DownScaleBy2( Image_s* imageDown, const Image_s* imageUp );
u32		Image_GetSize( Image_s* image );
bool	Image_ReadTga( Image_s* image, IStreamReader* reader, IAllocator* allocator );
void	Image_Release( Image_s* image );
void	Image_WriteBitmap( Image_s* image, IStreamWriter* stream );

void	ImageBC1_Create( ImageBC1_s* imageBC1, IAllocator* allocator, u32 width, u32 height );
void	ImageBC1_CreateWithData( ImageBC1_s* imageBC1, ImageBlockBC1_s* blocks, u32 width, u32 height );
void	ImageBC1_Encode( ImageBC1_s* imageBC1, const Image_s* image );
void	ImageBC1_Decode( Image_s* image, const ImageBC1_s* imageBC1 );
u32		ImageBC1_GetBlockCountFromDimension( u32 w, u32 h );
u32		ImageBC1_GetSize( const ImageBC1_s* imageBC1 );
u32		ImageBC1_GetSizeFromDimension( u32 w, u32 h );
void	ImageBC1_Release( ImageBC1_s* imageBC1 );
	
END_V6_NAMESPACE

#endif // __V6_CORE_IMAGE_H__