/*V6*/

#pragma once

#ifndef __V6_CORE_IMAGE_H__
#define __V6_CORE_IMAGE_H__

BEGIN_V6_NAMESPACE

class IAllocator;
class IStreamReader;
class IStreamWriter;
struct Color_s;

struct Image_s
{
	IAllocator*	heap;
	Color_s *	pixels;
	u32			width;
	u32			height;
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
void	Image_Create( Image_s* image, IAllocator* heap, u32 width, u32 height );
u32		Image_GetSize( Image_s* image );
bool	Image_ReadTga( Image_s* image, IStreamReader* reader, IAllocator* allocator );
void	Image_Release( Image_s* image );
void	Image_WriteBitmap( Image_s* image, IStreamWriter* stream );
	
END_V6_NAMESPACE

#endif // __V6_CORE_IMAGE_H__