/*V6*/

#include <v6/viewer/common.h>
#include <v6/viewer/tga_reader.h>

#include <v6/core/color.h>
#include <v6/core/image.h>
#include <v6/core/memory.h>

BEGIN_V6_VIEWER_NAMESPACE

#pragma pack( push, 1 )
struct TgaHeader_s
{
   char			idlength;
   char			colourmaptype;
   char			datatypecode;
   short int	colourmaporigin;
   short int	colourmaplength;
   char			colourmapdepth;
   short int	x_origin;
   short int	y_origin;
   short		width;
   short		height;
   char			bitsperpixel;
   char			imagedescriptor;
};
#pragma pack( pop )

void MergeBytes( core::Color_s *pixel, unsigned char *p, int bytes )
{
	switch (bytes)
	{
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

bool Tga_ReadFromFile( core::Image_s* image, const char* filenameTGA, core::IAllocator* allocator )
{
	FILE* fileTGA = nullptr;
	if ( fopen_s( &fileTGA, filenameTGA, "rb" ) != 0 )
		return false;

	TgaHeader_s header;
	core::u32 itemRead = fread( &header, sizeof(header), 1, fileTGA );
	V6_ASSERT( itemRead == 1 );

	if ( header.datatypecode != 2 && header.datatypecode != 10 )
	{
		V6_ASSERT_ALWAYS( "Can only handle image type 2 and 10" );
		return false;
	}

	if ( header.bitsperpixel != 16 && header.bitsperpixel != 24 && header.bitsperpixel != 32 )
	{
		V6_ASSERT_ALWAYS( "Can only handle pixel depths of 16, 24, and 32" );
		return false;
	}

	if ( header.colourmaptype != 0 && header.colourmaptype != 1 )
	{
		V6_ASSERT_ALWAYS( "Can only handle colour map types of 0 and 1" );
		return false;
	}

	core::Image_Create( image, allocator, header.width, header.height );;
	core::u32 pixelCount = core::Image_GetSize( image );
	core::Image_Clear( image );
	
	core::u32 skipover = 0;
	skipover += header.idlength;
	skipover += header.colourmaptype * header.colourmaplength;
	fseek( fileTGA, skipover, SEEK_CUR );
	
	const core::u32 bytesPerPixel = header.bitsperpixel / 8;
	for ( core::u32 pixelID = 0; pixelID < pixelCount; )
	{
		// Uncompressed
		if ( header.datatypecode == 2 )
		{                     
			core::u8 p[5];
			const core::u32 byteRead = fread( p, 1, bytesPerPixel, fileTGA );
			V6_ASSERT( byteRead == bytesPerPixel );
			MergeBytes( &image->pixels[pixelID], p, bytesPerPixel );
			++pixelID;
		}
		// Compressed
		else if ( header.datatypecode == 10 )
		{   
			core::u8 p[5];
			const core::u32 byteRead = fread( p, 1, bytesPerPixel+1, fileTGA );
			V6_ASSERT( byteRead == bytesPerPixel+1 );
			core::u32 j = p[0] & 0x7f;
			MergeBytes( &image->pixels[pixelID], &p[1], bytesPerPixel );
			++pixelID;

			// RLE chunk
			if ( p[0] & 0x80 )
			{
				for ( core::u32 i = 0; i < j; ++i )
				{
					MergeBytes( &image->pixels[pixelID], &p[1], bytesPerPixel );
					++pixelID;
				}
			}
			// Normal chunk
			else
			{                   
				for ( core::u32 i = 0; i < j; ++i )
				{
					const core::u32 byteRead = fread( p, 1, bytesPerPixel, fileTGA );
					V6_ASSERT( byteRead == bytesPerPixel );
					MergeBytes( &image->pixels[pixelID], p, bytesPerPixel );
					++pixelID;
				}
            }
		}
	}
   
	fclose( fileTGA );
	return true;
}

END_V6_VIEWER_NAMESPACE
