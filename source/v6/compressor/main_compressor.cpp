/*V6*/

#include <v6/core/common.h>
#include <v6/core/color.h>
#include <v6/core/compression.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

using namespace v6;

struct ImageBlock_s
{
	core::u16 color0;
	core::u16 color1;
	core::u32 bits;
};

int CompareRGBAByAlpha( const void* rgba1, const void* rgba2 )
{
	const int a1 = *((core::u32*)rgba1) & 0xFF;
	const int a2 = *((core::u32*)rgba2) & 0xFF;
	return a1 - a2;
}

void ImageBlock_EncodeBC1( ImageBlock_s* block, const core::Color_s* pixels, core::u32 lineStride )
{
	// Find the min/max colors
	
	core::Color_s min = core::Color_Make( 255, 255, 255, 255 );
	core::Color_s max = core::Color_Make(   0,   0,   0,   0 );

	for ( core::u32 y = 0; y < 4; ++y )
	{
		const core::u32 xOffset = y * lineStride;
		for ( core::u32 x = 0; x < 4; ++x )
		{
			const core::Color_s* pixel = &pixels[xOffset + x];

			min.r = core::Min( min.r, pixel->r );
			min.g = core::Min( min.g, pixel->g );
			min.b = core::Min( min.b, pixel->b );

			max.r = core::Max( max.r, pixel->r );
			max.g = core::Max( max.g, pixel->g );
			max.b = core::Max( max.b, pixel->b );
		}
	}

	const core::u32 extentR = (max.r - min.r) >> 4;
	const core::u32 extentG = (max.g - min.g) >> 4;
	const core::u32 extentB = (max.b - min.b) >> 4;

	min.r = min.r + extentR < 255 ? (min.r + extentR) : 255;
	min.g = min.g + extentG < 255 ? (min.g + extentG) : 255;
	min.b = min.b + extentB < 255 ? (min.b + extentB) : 255;

	max.r = max.r > extentR ? (max.r - extentR) : 0;
	max.g = max.g > extentG ? (max.g - extentG) : 0;
	max.b = max.b > extentB ? (max.b - extentB) : 0;

	// Output colors

	block->color0 = ((max.r >> 3) << 11) | ((max.g >> 2) << 5) | (max.b >> 3);
	block->color1 = ((min.r >> 3) << 11) | ((min.g >> 2) << 5) | (min.b >> 3);

	// Make palette

	core::Color_s colors[4];

	colors[0].r = (max.r & 0xF8) | (max.r >> 5);
	colors[0].g = (max.g & 0xFC) | (max.g >> 6);
	colors[0].b = (max.b & 0xF8) | (max.b >> 5);
	
	colors[1].r = (min.r & 0xF8) | (min.r >> 5);
	colors[1].g = (min.g & 0xFC) | (min.g >> 6);
	colors[1].b = (min.b & 0xF8) | (min.b >> 5);
	
	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

	// Ouput bits

	core::u32 shift = 0;
	block->bits = 0;

	for ( core::u32 y = 0; y < 4; ++y )
	{
		const core::u32 xOffset = y * lineStride;
		for ( core::u32 x = 0; x < 4; ++x )
		{
			const core::Color_s* pixel = &pixels[xOffset + x];

			core::u32 bestColorID = 0;
			core::u32 bestError = INT_MAX;
			
			for ( core::u32 colorID = 0; colorID < 4; ++colorID )
			{
				const int dR = pixel->r - colors[colorID].r;
				const int dG = pixel->g - colors[colorID].g;
				const int dB = pixel->b - colors[colorID].b;

				const core::u32 error = dR * dR + dG * dG + dB * dB;
				if ( error < bestError )
				{
					bestError = error;
					bestColorID = colorID;
				}
			}

			block->bits |= bestColorID << shift;
			shift += 2;
		}
	}
}

void ImageBlock_DecodeBC1( core::Color_s* pixels, core::u32 lineStride, const ImageBlock_s* block )
{
	// Decode min/max

	core::Color_s min = {};
	core::Color_s max = {};

	max.r = ((block->color0 >> 11) & 0x1F) << 3;
	max.g = ((block->color0 >>  5) & 0x3F) << 2;
	max.b = ((block->color0 >>  0) & 0x1F) << 3;

	min.r = ((block->color1 >> 11) & 0x1F) << 3;
	min.g = ((block->color1 >>  5) & 0x3F) << 2;
	min.b = ((block->color1 >>  0) & 0x1F) << 3;
	
	// Make palette

	core::Color_s colors[4] = {};
	
	colors[0].r = max.r | (max.r >> 5);
	colors[0].g = max.g | (max.g >> 6);
	colors[0].b = max.b | (max.b >> 5);
	
	colors[1].r = min.r | (min.r >> 5);
	colors[1].g = min.g | (min.g >> 6);
	colors[1].b = min.b | (min.b >> 5);
	
	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

	// Decode bits

	core::u32 shift = 0;
	
	for ( core::u32 y = 0; y < 4; ++y )
	{
		const core::u32 xOffset = y * lineStride;
		for ( core::u32 x = 0; x < 4; ++x )
		{
			const core::u32 colorID = (block->bits >> shift) & 3;
			pixels[xOffset + x] = colors[colorID];
			shift += 2;
		}
	}
}

void TestImageCompression( core::IAllocator* allocator )
{
	//const char* filenameSrc = "D:/media/image/femme.tga";
	//const char* filenameSrc = "D:/media/image/montagne.tga";
	//const char* filenameSrc = "D:/media/image/ville.tga";
	//const char* filenameSrc = "D:/media/image/plage.tga";
	//const char* filenameSrc = "D:/media/image/rgb.tga";
	const char* filenameSrc = "D:/media/image/sponza01.tga";
	core::Image_s imageSrc = {};
	core::CFileReader fileReader;
	if ( !fileReader.Open( filenameSrc ) || !core::Image_ReadTga( &imageSrc, &fileReader, allocator ) )
	{
		V6_ERROR( "Unable to read %s\n", filenameSrc );
		return;
	}

	V6_ASSERT( core::IsPowOfTwo( imageSrc.width ) );
	V6_ASSERT( core::IsPowOfTwo( imageSrc.height ) );
	V6_MSG( "Compressing image %dx%d...\n", imageSrc.width, imageSrc.height );

	core::Image_s imageDst = {};
	Image_Create( &imageDst, allocator, imageSrc.width, imageSrc.height );

	for ( core::u32 y = 0; y < imageSrc.height; y += 4 )
	{
		const core::u32 xOffset = y * imageSrc.width;
		for ( core::u32 x = 0; x < imageSrc.width; x += 4 )
		{
			ImageBlock_s block;
			ImageBlock_EncodeBC1( &block, &imageSrc.pixels[xOffset + x], imageSrc.width );
			ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, &block );			
		}
	}

	//const char* filenameDst = "D:/media/image/femme_bc1.bmp";
	//const char* filenameDst = "D:/media/image/montagne_bc1.bmp";
	//const char* filenameDst = "D:/media/image/ville_bc1.bmp";
	//const char* filenameDst = "D:/media/image/plage_bc1.bmp";
	//const char* filenameDst = "D:/media/image/rgb_bc1.bmp";
	const char* filenameDst = "D:/media/image/sponza01_bc1.bmp";
	core::CFileWriter fileWriter;
	if ( !fileWriter.Open( filenameDst ) )
	{
		V6_ERROR( "Unable to write %s\n", filenameDst );
		return;
	}	
	
	Image_WriteBitmap( &imageDst, &fileWriter );

	V6_MSG( "Compressing done.\n" );
}

void TestBlockCompression( core::IAllocator* allocator )
{
	static const core::u32 testCount = 4;

	for ( core::u32 testID = 0; testID < testCount; ++testID )
	{
		core::u32 cellCount = 0;

		core::u32 cellRGBA[64] = {};
		memset( cellRGBA, 0xFF, sizeof( cellRGBA ) );

		while ( cellCount < 64 )
		{
retry:
			const core::u32 cellPos = rand() & 0x3F;
			
			for ( core::u32 cellRank = 0; cellRank < cellCount; ++cellRank )
			{
				if ( (cellRGBA[cellRank] & 0xFF) == cellPos )
					goto retry;
			}

			cellRGBA[cellCount] = ((rand() & 0xFF) << 24) | ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | cellPos;
			++cellCount;
		}

		core::EncodedBlockEx_s encodedBlock;
		core::Block_Encode( &encodedBlock, cellRGBA, cellCount );

		core::u32 decodedCellRGBA[64] = {};
		core::u32 decodedCellCount = 0;
		core::Block_Decode( decodedCellRGBA, &decodedCellCount, &encodedBlock );

		qsort( cellRGBA, cellCount, sizeof( *cellRGBA ), CompareRGBAByAlpha );
		qsort( decodedCellRGBA, cellCount, sizeof( *decodedCellRGBA ), CompareRGBAByAlpha );

		printf( "%d cells:\n", cellCount );
		for ( core::u32 cellID = 0; cellID < cellCount; ++cellID )
		{
			const core::u32 color1R  = (cellRGBA[cellID] >> 24) & 0xFF;
			const core::u32 color1G  = (cellRGBA[cellID] >> 16) & 0xFF;
			const core::u32 color1B  = (cellRGBA[cellID] >>  8) & 0xFF;
			const core::u32 cellpos1 = cellRGBA[cellID] & 0xFF;

			const core::u32 color2R  = (decodedCellRGBA[cellID] >> 24) & 0xFF;
			const core::u32 color2G  = (decodedCellRGBA[cellID] >> 16) & 0xFF;
			const core::u32 color2B  = (decodedCellRGBA[cellID] >>  8) & 0xFF;
			const core::u32 cellpos2 = decodedCellRGBA[cellID] & 0xFF;

			V6_ASSERT( cellpos1 == cellpos2 );
			printf( "%02X %02X %02X %02X => %02X %02X %02X %02X\n", 
				color1R, color1G, color1B, cellpos1,
				color2R, color2G, color2B, cellpos2 );
		}
	}
}

int main()
{
	V6_MSG( "Compressor 0.0\n" );

	core::CHeap heap;
	core::Stack stack( &heap, 100 * 1024 * 1024 );
		
	// TestImageCompression( &stack );

	TestBlockCompression( &stack );

	return 0;
}