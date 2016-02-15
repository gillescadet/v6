/*V6*/

#pragma comment(lib, "core.lib")

#include <math.h>

#include <v6/core/common.h>
#include <v6/core/color.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

using namespace v6;

#define rgb	AsRGB()
#define xyx	AsXYZ()

typedef core::u32 uint; 

struct uint3
{
	uint3() {}
	uint3( uint v0, uint v1, uint v2 ) : x( v0 ), y( v1 ), z( v2 ) {}
	union
	{
		struct
		{
			uint x;
			uint y;
			uint z;
		};
		struct
		{
			uint r;
			uint g;
			uint b;
		};
	};
};

struct uint4
{
	uint4() {}
	uint4( uint v0, uint v1, uint v2, uint v3 ) : x( v0 ), y( v1 ), z( v2 ), w( v3 ) {}
	uint4( uint3 v0, uint v1 ) : x( v0.x ), y( v0.y ), z( v0.z ), w( v1 ) {}
	union
	{
		struct
		{
			uint x;
			uint y;
			uint z;
			uint w;
		};
		struct
		{
			uint r;
			uint g;
			uint b;
			uint a;
		};
	};
	uint3& AsRGB() { return *((uint3*)&x); }
	const uint3& AsRGB() const { return *((uint3*)&x); }
	uint3& AsXYZ() { return *((uint3*)&x); }
	const uint3& AsXYZ() const { return *((uint3*)&x); }
};

struct ImageBlock_s
{
	core::u16 color0;
	core::u16 color1;
	core::u32 bits;
};

struct EncodedBlock
{
	uint	blockPos;
	uint	cellEndColors;
	uint	cellColorIndices[4];
	uint	cellPresence[2];
};

struct DecodedBlock
{
	uint	blockPos;
	uint	cellRGBA[64];
	uint	cellCount;
};

inline uint min( uint a, uint b )
{
	return a < b ? a : b;
}

inline uint max( uint a, uint b )
{
	return a > b ? a : b;
}

inline uint3 min( uint3 a, uint3 b )
{
	uint3 res;
	res.x = min( a.x, b.x );
	res.y = min( a.y, b.y );
	res.z = min( a.z, b.z );
	return res;
}

inline uint3 max( uint3 a, uint3 b )
{
	uint3 res;
	res.x = max( a.x, b.x );
	res.y = max( a.y, b.y );
	res.z = max( a.z, b.z );
	return res;
}

inline uint3 operator-( uint3 a, uint3 b )
{
	uint3 res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	res.z = a.z - b.z;
	return res;
}

inline uint3 operator>>( uint3 a, uint shift )
{
	uint3 res;
	res.x = a.x >> shift;
	res.y = a.y >> shift;
	res.z = a.z >> shift;
	return res;
}

uint4 UnpackRGBA( uint rgba )
{
	return uint4( (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF );
}

uint PackRGBA( uint4 rgba )
{
	return (rgba.r << 24) | (rgba.g << 16) | (rgba.b << 8) | rgba.a;
}

int CompareRGBAByAlpha( const void* rgba1, const void* rgba2 )
{
	return UnpackRGBA( *(uint*)rgba1 ).a - UnpackRGBA( *(uint*)rgba2 ).a;
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

EncodedBlock Block_Encode( uint blockPos, uint cellRGBA[64], uint cellCount )
{
	EncodedBlock block;

	// Output blockPos
	
	block.blockPos = blockPos;

	// Find the min/max colors
	
	uint3 minColor = uint3( 255, 255, 255 );
	uint3 maxColor = uint3(   0,   0,   0 );

	{
		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint4 color = UnpackRGBA( cellRGBA[cellID] );

			minColor = min( minColor, color.rgb );
			maxColor = max( maxColor, color.rgb );
		}	

		const uint3 extentColor = (maxColor - minColor) >> 4;

		minColor.r = minColor.r + extentColor.r < 255 ? (minColor.r + extentColor.r) : 255;
		minColor.g = minColor.g + extentColor.g < 255 ? (minColor.g + extentColor.g) : 255;
		minColor.b = minColor.b + extentColor.b < 255 ? (minColor.b + extentColor.b) : 255;

		maxColor.r = maxColor.r > extentColor.r ? (maxColor.r - extentColor.r) : 0;
		maxColor.g = maxColor.g > extentColor.g ? (maxColor.g - extentColor.g) : 0;
		maxColor.b = maxColor.b > extentColor.b ? (maxColor.b - extentColor.b) : 0;
	}

	// Output colors

	const uint color0 = ((maxColor.r >> 3) << 11) | ((maxColor.g >> 2) << 5) | (maxColor.b >> 3);
	const uint color1 = ((minColor.r >> 3) << 11) | ((minColor.g >> 2) << 5) | (minColor.b >> 3);
	block.cellEndColors = (color1 << 16) | color0;

	// Make palette

	uint3 colors[4];

	colors[0].r = (maxColor.r & 0xF8) | (maxColor.r >> 5);
	colors[0].g = (maxColor.g & 0xFC) | (maxColor.g >> 6);
	colors[0].b = (maxColor.b & 0xF8) | (maxColor.b >> 5);

	colors[1].r = (minColor.r & 0xF8) | (minColor.r >> 5);
	colors[1].g = (minColor.g & 0xFC) | (minColor.g >> 6);
	colors[1].b = (minColor.b & 0xF8) | (minColor.b >> 5);
	
	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

	// Output cell presence

	block.cellPresence[0] = 0;
	block.cellPresence[1] = 0;

	uint cellPosToID[64];

	{
		for ( uint cellPos = 0; cellPos < 64; ++cellPos )
			cellPosToID[cellPos] = (uint)-1;

		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint cellPos = UnpackRGBA( cellRGBA[cellID] ).a;
			block.cellPresence[cellPos >> 5] |= 1 << (cellPos & 0x1F);
			cellPosToID[cellPos] = cellID;
		}
	}

	// Ouput cell colors

	block.cellColorIndices[0] = 0;
	block.cellColorIndices[1] = 0;
	block.cellColorIndices[2] = 0;
	block.cellColorIndices[3] = 0;
	
	{
		uint cellRank = 0;
		for ( uint cellPos = 0; cellPos < 64; ++cellPos )
		{
			if ( cellPosToID[cellPos] == (uint)-1 )
				continue;

			const uint cellID = cellPosToID[cellPos];
			const uint4 color = UnpackRGBA( cellRGBA[cellID] );

			uint bestColorID = 0;
			uint bestError = 0xFFFFFFFF;
			
			for ( uint colorID = 0; colorID < 4; ++colorID )
			{
				const int dR = color.r - colors[colorID].r;
				const int dG = color.g - colors[colorID].g;
				const int dB = color.b - colors[colorID].b;

				const uint error = dR * dR + dG * dG + dB * dB;
				if ( error < bestError )
				{
					bestError = error;
					bestColorID = colorID;
				}
			}

			block.cellColorIndices[cellRank >> 4] |= bestColorID << ((cellRank << 1) & 0x1F);
			++cellRank;
		}
	}

	return block;
}

DecodedBlock Block_Decode( EncodedBlock encodedBlock )
{
	DecodedBlock decodedBlock;

	// Decode blockPos

	decodedBlock.blockPos = encodedBlock.blockPos;

	// Decode min/max

	const uint color0 = (encodedBlock.cellEndColors >> 0 ) & 0xFFFF;
	const uint color1 = (encodedBlock.cellEndColors >> 16) & 0xFFFF;

	uint3 maxColor;
	maxColor.r = ((color0 >> 11) & 0x1F) << 3;
	maxColor.g = ((color0 >>  5) & 0x3F) << 2;
	maxColor.b = ((color0 >>  0) & 0x1F) << 3;
	
	uint3 minColor;
	minColor.r = ((color1 >> 11) & 0x1F) << 3;
	minColor.g = ((color1 >>  5) & 0x3F) << 2;
	minColor.b = ((color1 >>  0) & 0x1F) << 3;

	// Make palette

	uint3 colors[4];
	
	colors[0].r = maxColor.r | (maxColor.r >> 5);
	colors[0].g = maxColor.g | (maxColor.g >> 6);
	colors[0].b = maxColor.b | (maxColor.b >> 5);

	colors[1].r = minColor.r | (minColor.r >> 5);
	colors[1].g = minColor.g | (minColor.g >> 6);
	colors[1].b = minColor.b | (minColor.b >> 5);
	
	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;	
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

	// Decode bits

	decodedBlock.cellCount = 0;
	
	for ( uint cellPos = 0; cellPos < 64; ++cellPos )
	{
		if ( (encodedBlock.cellPresence[cellPos >> 5] & (1 << (cellPos & 0x1F))) == 0 )
			continue;

		const uint cellRank = decodedBlock.cellCount;
		const uint colorID = (encodedBlock.cellColorIndices[cellRank >> 4] >> ((cellRank << 1) & 0x1F)) & 3;
		decodedBlock.cellRGBA[cellRank] = PackRGBA( uint4( colors[colorID], cellPos ) );
		++decodedBlock.cellCount;
	}

	return decodedBlock;
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
	static const uint testCount = 16;

	for ( uint testID = 0; testID < testCount; ++testID )	
	{
		uint cellCount = 0;

		uint cellRGBA[64] = {};
		memset( cellRGBA, 0xFF, sizeof( cellRGBA ) );

		while ( cellCount < 16 )
		{
retry:
			const uint cellPos = rand() & 0x3F;
			
			for ( uint cellRank = 0; cellRank < 64; ++cellRank )
			{
				if ( UnpackRGBA( cellRGBA[cellRank] ).a == cellPos )
					goto retry;
			}

			cellRGBA[cellCount] = PackRGBA( uint4( rand() & 0xFF, rand() & 0xFF, rand() & 0xFF, cellPos ) );
			++cellCount;
		}

		EncodedBlock encodedBlock = Block_Encode( 0, cellRGBA, cellCount );
		DecodedBlock decodedBlock = Block_Decode( encodedBlock );

		qsort( cellRGBA, cellCount, sizeof( *cellRGBA ), CompareRGBAByAlpha );
		qsort( decodedBlock.cellRGBA, cellCount, sizeof( *cellRGBA ), CompareRGBAByAlpha );

		printf( "%d cells:\n", cellCount );
		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint4 rgba1 = UnpackRGBA( cellRGBA[cellID] );
			const uint4 rgba2 = UnpackRGBA( decodedBlock.cellRGBA[cellID] );
			V6_ASSERT( rgba1.a == rgba2.a );
			printf( "%02X %02X %02X %02X => %02X %02X %02X %02X\n", 
				rgba1.r, rgba1.g, rgba1.b, rgba1.a,
				rgba2.r, rgba2.g, rgba2.b, rgba2.a );
		}
	}
}

int main()
{
	V6_MSG( "Compressor 0.0\n" );

	core::CHeap heap;
	core::Stack stack( &heap, 100 * 1024 * 1024 );
		
	TestImageCompression( &stack );

	// TestBlockCompression( &stack );

	return 0;
}