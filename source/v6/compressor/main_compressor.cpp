/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/core/color.h>
#include <v6/core/image.h>
#include <v6/core/mat3x3.h>
#include <v6/core/math.h>
#include <v6/core/memory.h>
#include <v6/core/filesystem.h>
#include <v6/core/optimization.h>
#include <v6/core/plot.h>
#include <v6/core/random.h>
#include <v6/core/stream.h>
#include <v6/core/string.h>
#include <v6/core/time.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>

#define MAIN_COMPRESSOR_DEBUG 0

BEGIN_V6_NAMESPACE

struct ImageBlock_s
{
	u16 color0;
	u16 color1;
	u32 bits;
};

struct RawBlock_s
{
	u32 cellRGBA[CODEC_CELL_MAX_COUNT];
	u32 cellCount;
};

#if MAIN_COMPRESSOR_DEBUG == 1
Plot_s* s_plot = nullptr;
#endif // #if MAIN_COMPRESSOR_DEBUG == 1

//----------------------------------------------------------------------------------------------------

void OutputMessage( const char * format, ... )
{
	char buffer[4096];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof( buffer ), format, args);
	va_end( args );

	fputs( buffer, stdout );
}

//----------------------------------------------------------------------------------------------------

static int CompareRGBAByAlpha( const void* rgba1, const void* rgba2 )
{
	const int a1 = *((u32*)rgba1) & 0xFF;
	const int a2 = *((u32*)rgba2) & 0xFF;
	return a1 - a2;
}

static float ImageBlock_Error( const Color_s* originalPixels, const Color_s* decodedPixels, u32 lineStride )
{
	float sumMSE = 0.0f;
	for ( u32 y = 0; y < 4; ++y )
	{
		const u32 xOffset = y * lineStride;
		for ( u32 x = 0; x < 4; ++x )
		{
			const Color_s* originalPixel = &originalPixels[xOffset + x];
			const Color_s* decodedPixel = &decodedPixels[xOffset + x];

			const Vec3i vdiff = Vec3i_Make( decodedPixel->r - originalPixel->r, decodedPixel->g - originalPixel->g, decodedPixel->b - originalPixel->b );
			sumMSE += vdiff.LengthSq() / 3.0f;
		}
	}

	return sumMSE;
}

static u32 ImageBlock_EncodeBC1_Build( ImageBlock_s* block, const Color_s* pixels, u32 lineStride, Color_s endColor0, Color_s endColor1, u32 binCount )
{
	// Output colors

	block->color0 = ((endColor0.r >> 3) << 11) | ((endColor0.g >> 2) << 5) | (endColor0.b >> 3);
	block->color1 = ((endColor1.r >> 3) << 11) | ((endColor1.g >> 2) << 5) | (endColor1.b >> 3);

	if ( (block->color0 > block->color1) != (binCount == 4) )
	{
		Swap( block->color0, block->color1 );
		Swap( endColor0, endColor1 );
	}

	// Make palette

	Color_s colors[4];

	colors[0].r = (endColor0.r & 0xF8) | (endColor0.r >> 5);
	colors[0].g = (endColor0.g & 0xFC) | (endColor0.g >> 6);
	colors[0].b = (endColor0.b & 0xF8) | (endColor0.b >> 5);
	
	colors[1].r = (endColor1.r & 0xF8) | (endColor1.r >> 5);
	colors[1].g = (endColor1.g & 0xFC) | (endColor1.g >> 6);
	colors[1].b = (endColor1.b & 0xF8) | (endColor1.b >> 5);

	if ( binCount == 4 )
	{
		colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
		colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
		colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
		colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
		colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
		colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;
	}
	else
	{
		V6_ASSERT( binCount == 3 );
		colors[2].r = (colors[0].r + colors[1].r) >> 1;
		colors[2].g = (colors[0].g + colors[1].g) >> 1;
		colors[2].b = (colors[0].b + colors[1].b) >> 1;
	}

	// Ouput bits

	block->bits = 0;
	u32 shift = 0;
	u32 sumError = 0;

	for ( u32 y = 0; y < 4; ++y )
	{
		const u32 xOffset = y * lineStride;
		for ( u32 x = 0; x < 4; ++x )
		{
			const Color_s* pixel = &pixels[xOffset + x];

			u32 bestColorID = 0;
			u32 bestError = INT_MAX;
			
			for ( u32 colorID = 0; colorID < binCount; ++colorID )
			{
				const int dR = pixel->r - colors[colorID].r;
				const int dG = pixel->g - colors[colorID].g;
				const int dB = pixel->b - colors[colorID].b;

				const u32 error = dR * dR + dG * dG + dB * dB;
				if ( error < bestError )
				{
					bestError = error;
					bestColorID = colorID;
				}
			}

			block->bits |= bestColorID << shift;
			shift += 2;
			sumError += bestError;
		}
	}

	return sumError;
}

static u32 ImageBlock_EncodeBC1_BoundingBox( ImageBlock_s* block, const Color_s* pixels, u32 lineStride )
{
	// Find the min/max colors
	
	Color_s min = Color_Make( 255, 255, 255, 255 );
	Color_s max = Color_Make(   0,   0,   0,   0 );

	for ( u32 y = 0; y < 4; ++y )
	{
		const u32 xOffset = y * lineStride;
		for ( u32 x = 0; x < 4; ++x )
		{
			const Color_s* pixel = &pixels[xOffset + x];

			min.r = Min( min.r, pixel->r );
			min.g = Min( min.g, pixel->g );
			min.b = Min( min.b, pixel->b );

			max.r = Max( max.r, pixel->r );
			max.g = Max( max.g, pixel->g );
			max.b = Max( max.b, pixel->b );
		}
	}

	const u32 extentR = (max.r - min.r) >> 4;
	const u32 extentG = (max.g - min.g) >> 4;
	const u32 extentB = (max.b - min.b) >> 4;

	min.r = min.r + extentR < 255 ? (min.r + extentR) : 255;
	min.g = min.g + extentG < 255 ? (min.g + extentG) : 255;
	min.b = min.b + extentB < 255 ? (min.b + extentB) : 255;

	max.r = max.r > extentR ? (max.r - extentR) : 0;
	max.g = max.g > extentG ? (max.g - extentG) : 0;
	max.b = max.b > extentB ? (max.b - extentB) : 0;

	return ImageBlock_EncodeBC1_Build( block, pixels, lineStride, min, max, 4 );
}

static void ImageBlock_EncodeBC1_BruteForce( ImageBlock_s* block, const Color_s* pixels, u32 lineStride, u32 radius )
{
	// Find the min/max colors
	
	Color_s min = Color_Make( 255, 255, 255, 255 );
	Color_s max = Color_Make(   0,   0,   0,   0 );

	for ( u32 y = 0; y < 4; ++y )
	{
		const u32 xOffset = y * lineStride;
		for ( u32 x = 0; x < 4; ++x )
		{
			const Color_s* pixel = &pixels[xOffset + x];

			min.r = Min( min.r, pixel->r );
			min.g = Min( min.g, pixel->g );
			min.b = Min( min.b, pixel->b );

			max.r = Max( max.r, pixel->r );
			max.g = Max( max.g, pixel->g );
			max.b = Max( max.b, pixel->b );
		}
	}

	min.r >>= 3;
	min.g >>= 2;
	min.b >>= 3;

	max.r >>= 3;
	max.g >>= 2;
	max.b >>= 3;
	
	Vec3u startMin, endMin;
	startMin.x = Max( 0, (int)min.r - (int)radius );
	startMin.y = Max( 0, (int)min.g - (int)radius );
	startMin.z = Max( 0, (int)min.b - (int)radius );
	endMin.x = Min( 31, (int)min.r + (int)radius );
	endMin.y = Min( 63, (int)min.g + (int)radius );
	endMin.z = Min( 31, (int)min.b + (int)radius );

	Vec3u startMax, endMax;
	startMax.x = Max( 0, (int)max.r - (int)radius );
	startMax.y = Max( 0, (int)max.g - (int)radius );
	startMax.z = Max( 0, (int)max.b - (int)radius );
	endMax.x = Min( 31, (int)max.r + (int)radius );
	endMax.y = Min( 63, (int)max.g + (int)radius );
	endMax.z = Min( 31, (int)max.b + (int)radius );

	u32 minError = INT_MAX;

	for ( min.r = startMin.x; min.r <= endMin.x; ++min.r )
	{
		for ( min.g = startMin.y; min.g <= endMin.y; ++min.g )
		{
			for ( min.b = startMin.z; min.b <= endMin.z; ++min.b )
			{
				for ( max.r = startMax.x; max.r <= endMax.x; ++max.r )
				{
					for ( max.g = startMax.y; max.g <= endMax.y; ++max.g )
					{
						for ( max.b = startMax.z; max.b <= endMax.z; ++max.b )
						{
							ImageBlock_s curBlock;
							const u32 error = ImageBlock_EncodeBC1_Build( &curBlock, pixels, lineStride, min, max, 4 );

							if ( error < minError )
							{
								minError = error;
								*block = curBlock;
							}
						}
					}
				}
			}
		}
	}
}

static int CompareColor( const void* pc0, const void* pc1 )
{
	const Color_s c0 = *((const Color_s*)pc0);
	const Color_s c1 = *((const Color_s*)pc1);
	return (int)c0.bits - (int)c1.bits;
}

static void EndColor_F32ToU8( Color_s* endColorU8, const Vec3* endColorF32 )
{
	endColorU8->r = (u8)Clamp( endColorF32->x * 255.0f, 0.0f, 255.0f );
	endColorU8->g = (u8)Clamp( endColorF32->y * 255.0f, 0.0f, 255.0f );
	endColorU8->b = (u8)Clamp( endColorF32->z * 255.0f, 0.0f, 255.0f );
	endColorU8->a = 0;

	endColorU8->r = (endColorU8->r & ~7) | (endColorU8->r >> 5);
	endColorU8->g = (endColorU8->g & ~3) | (endColorU8->g >> 6);
	endColorU8->b = (endColorU8->b & ~7) | (endColorU8->b >> 5);
}

static void EndColor_U8ToF32( Vec3* endColorF32, const Color_s* endColorU8 )
{
	endColorF32->x = endColorU8->r / 255.0f;
	endColorF32->y = endColorU8->g / 255.0f;
	endColorF32->z = endColorU8->b / 255.0f;
}

template < int RADIUS >
static u32 ImageBlock_EncodeBC1_Refine( ImageBlock_s* block, const Color_s* pixels, u32 lineStride, const Vec3* endColor0, const Vec3* endColor1, u32 binCount )
{
	// Converts to 8 bits
	
	Color_s min;
	min.r = (u8)Clamp( endColor0->x * 255.0f, 0.0f, 255.0f );
	min.g = (u8)Clamp( endColor0->y * 255.0f, 0.0f, 255.0f );
	min.b = (u8)Clamp( endColor0->z * 255.0f, 0.0f, 255.0f );

	Color_s max;
	max.r = (u8)Clamp( endColor1->x * 255.0f, 0.0f, 255.0f );
	max.g = (u8)Clamp( endColor1->y * 255.0f, 0.0f, 255.0f );
	max.b = (u8)Clamp( endColor1->z * 255.0f, 0.0f, 255.0f );

	Vec3u minHighBits;
	minHighBits.x = min.r >> 5;
	minHighBits.y = min.g >> 6;
	minHighBits.z = min.b >> 5;

	Vec3u maxHighBits;
	maxHighBits.x = max.r >> 5;
	maxHighBits.y = max.g >> 6;
	maxHighBits.z = max.b >> 5;

	min.r >>= 3;
	min.g >>= 2;
	min.b >>= 3;

	max.r >>= 3;
	max.g >>= 2;
	max.b >>= 3;
	
	Vec3u startMin, endMin;
	startMin.x = Max( 0, (int)min.r - RADIUS );
	startMin.y = Max( 0, (int)min.g - RADIUS );
	startMin.z = Max( 0, (int)min.b - RADIUS );
	endMin.x = Min( 31, (int)min.r + RADIUS );
	endMin.y = Min( 63, (int)min.g + RADIUS );
	endMin.z = Min( 31, (int)min.b + RADIUS );

	Vec3u startMax, endMax;
	startMax.x = Max( 0, (int)max.r - RADIUS );
	startMax.y = Max( 0, (int)max.g - RADIUS );
	startMax.z = Max( 0, (int)max.b - RADIUS );
	endMax.x = Min( 31, (int)max.r + RADIUS );
	endMax.y = Min( 63, (int)max.g + RADIUS );
	endMax.z = Min( 31, (int)max.b + RADIUS );

	u32 minError = INT_MAX;

	for ( min.r = startMin.x; min.r <= endMin.x; ++min.r )
	{
		for ( min.g = startMin.y; min.g <= endMin.y; ++min.g )
		{
			for ( min.b = startMin.z; min.b <= endMin.z; ++min.b )
			{
				for ( max.r = startMax.x; max.r <= endMax.x; ++max.r )
				{
					for ( max.g = startMax.y; max.g <= endMax.y; ++max.g )
					{
						for ( max.b = startMax.z; max.b <= endMax.z; ++max.b )
						{
							Color_s endColor0 = min;
							endColor0.r = (min.r << 3) | minHighBits.x;
							endColor0.g = (min.g << 2) | minHighBits.y;
							endColor0.b = (min.b << 3) | minHighBits.z;

							Color_s endColor1 = max;
							endColor1.r = (max.r << 3) | maxHighBits.x;
							endColor1.g = (max.g << 2) | maxHighBits.y;
							endColor1.b = (max.b << 3) | maxHighBits.z;

							ImageBlock_s curBlock;
							const u32 error = ImageBlock_EncodeBC1_Build( &curBlock, pixels, lineStride, endColor0, endColor1, binCount );

							if ( error < minError )
							{
								minError = error;
								*block = curBlock;
							}
						}
					}
				}
			}
		}
	}

	return minError;
}

static u32 ImageBlock_EncodeBC1_Optimize( ImageBlock_s* block, const Color_s* pixels, u32 lineStride, u32 binCount, u32 radius )
{
	V6_ASSERT( binCount == 3 || binCount == 4 );

	// Inspired by AMD compressonator: http://developer.amd.com/tools-and-sdks/archive/the-compressonator/

	// Build unique pixel array

	Color_s sortedPixels[16];
	u32 lastUniquePixelID = 0;

	{
		
		Color_s* sortedPixel = sortedPixels;
		for ( u32 y = 0; y < 4; ++y )
		{
			const u32 xOffset = y * lineStride;
			for ( u32 x = 0; x < 4; ++x, ++sortedPixel )
				*sortedPixel = pixels[xOffset + x];
		}

		qsort( sortedPixels, 16, sizeof( Color_s ), CompareColor );

		for ( u32 pixelID = 1; pixelID < 16; ++pixelID )
		{
			if ( sortedPixels[pixelID].bits != sortedPixels[lastUniquePixelID].bits )
			{
				++lastUniquePixelID;
				sortedPixels[lastUniquePixelID] = sortedPixels[pixelID];
			}
		}

		if ( lastUniquePixelID < 2 )
			return ImageBlock_EncodeBC1_Build( block, pixels, lineStride, sortedPixels[0], sortedPixels[lastUniquePixelID], binCount );
	}

	// Compute centred colors

	Vec3 colorCenter = Vec3_Zero();
	Vec3 centredColors[16];

	{
		Vec3 colors[16];
		u32 colorID = 0;
		for ( u32 y = 0; y < 4; ++y )
		{
			const u32 xOffset = y * lineStride;
			for ( u32 x = 0; x < 4; ++x, ++colorID )
			{
				const Color_s* pixel = &pixels[xOffset + x];
				colors[colorID].x = pixel->r / 255.0f;
				colors[colorID].y = pixel->g / 255.0f;
				colors[colorID].z = pixel->b / 255.0f;
				colorCenter += colors[colorID];
			}
		}

		colorCenter *= 1.0f / 16.0f;
	
		for ( u32 colorID = 0; colorID < 16; ++colorID )
			centredColors[colorID] = colors[colorID] - colorCenter;
	}

#if MAIN_COMPRESSOR_DEBUG == 1
	if ( s_plot )
	{
		Plot_NewObject( s_plot, Color_Make( 255, 255, 255, 255 ) );
		for ( u32 colorID = 0; colorID < 16; ++colorID )
			Plot_AddPoint( s_plot, &centredColors[colorID] );
	}
#endif // #if MAIN_COMPRESSOR_DEBUG == 1

	// Compute best line passing by centred colors

	static const float colorEps = 3.0f * (2.0f / 255.f) * (2.0f / 255.0f);
	Vec3 colorDir;

	{
		Mat3x3 covariance;
		Optimization_FindBestFittingLine3DPrecentred( &colorDir, &covariance, centredColors, 16 );

		if ( covariance.m_row0.x < FLT_EPSILON && covariance.m_row1.y < FLT_EPSILON && covariance.m_row2.z < FLT_EPSILON )
			return ImageBlock_EncodeBC1_Build( block, pixels, lineStride, sortedPixels[0], sortedPixels[lastUniquePixelID], binCount );
	}

	const float binMax = (float)(binCount - 1);
	const float invBinMax = 1.0f / binMax;
	float binIndexes[4];
	for ( u32 binID = 0; binID < binCount; ++binID )
		binIndexes[binID] = (binID * invBinMax) - 0.5f;

	Vec3 bestEndColor0;
	Vec3 bestEndColor1;
	float minSumDistanceSQ = FLT_MAX;

	for ( u32 pass = 0; pass < 8; ++pass )
	{
		// Project centred colors on line to find bounds

		float ts[16];
		float tMin = FLT_MAX;
		float tMax = -FLT_MAX;
		for ( u32 colorID = 0; colorID < 16; ++colorID )
		{
			ts[colorID] = Dot( centredColors[colorID], colorDir );
			tMin = Min( tMin, ts[colorID] );
			tMax = Max( tMax, ts[colorID] );
		}

#if MAIN_COMPRESSOR_DEBUG == 1
		if ( s_plot )
		{
			const Vec3 p0 = colorDir * tMin;
			const Vec3 p1 = colorDir * tMax;
			Plot_NewObject( s_plot, Color_Make( 255, 0, 0, 255 ) );
			Plot_AddLine( s_plot, &p0, &p1 );
		}
#endif // #if MAIN_COMPRESSOR_DEBUG == 1

		// Try multiple segments

		const float tLen = (tMax - tMin);
		const float tStep = tLen * 0.025f;

		u8 decodedBinIDBuffers[2][16];
		u32 decodedBinIDBuffer = 0;
		u8* bestDecodedBinIDs = nullptr; 

		for ( int step0 = -4; step0 <= 4; ++step0 )
		{
			const float t0 = tMin + step0 * tStep;
			for ( int step1 = -4; step1 <= 4; ++step1 )
			{
				const float t1 = tMax + step1 * tStep;
				const float tDiff = t1 - t0;
				if ( tDiff < colorEps )
					continue;

				Vec3 endColor0 = colorCenter + t0 * colorDir;
				Vec3 endColor1 = colorCenter + t1 * colorDir;
				if ( endColor0.Max() < 0.0f || endColor0.Min() > 1.0f || endColor1.Max() < 0.0f || endColor1.Min() > 1.0f )
					continue;

				{
					Color_s encodedColor0;
					Color_s encodedColor1;
					EndColor_F32ToU8( &encodedColor0, &endColor0 );
					EndColor_F32ToU8( &encodedColor1, &endColor1 );
					EndColor_U8ToF32( &endColor0, &encodedColor0 );
					EndColor_U8ToF32( &endColor1, &encodedColor1 );
				}

				// Project centred colors on segment to find decoded colors

				u8* decodedBinIDs = decodedBinIDBuffers[decodedBinIDBuffer];
				float segmentSumDistanceSQ = 0.0f;

				Vec3 binColors[4];
				binColors[0] = endColor0 - colorCenter;
				binColors[binCount-1] = endColor1 - colorCenter;
				for ( u32 binID = 1; binID < binCount-1; ++binID )
				{
					const float ratio = binID * invBinMax;
					binColors[binID] = Lerp( binColors[0], binColors[binCount-1], ratio );
				}
				
				for ( u32 colorID = 0; colorID < 16; ++colorID )
				{
					float bestDistanceSQ = FLT_MAX;
					for ( u32 binID = 0; binID < binCount; ++binID )
					{
						const float distanceSQ = (centredColors[colorID] - binColors[binID]).LengthSq();
						if ( distanceSQ < bestDistanceSQ )
						{
							bestDistanceSQ = distanceSQ;
							decodedBinIDs[colorID] = binID;
						}
					}
					
					segmentSumDistanceSQ += bestDistanceSQ;
				}

				// Keep the best segment

				if ( segmentSumDistanceSQ < minSumDistanceSQ )
				{
					minSumDistanceSQ = segmentSumDistanceSQ;
					bestEndColor0 = endColor0;
					bestEndColor1 = endColor1;
					bestDecodedBinIDs = decodedBinIDs;
					decodedBinIDBuffer ^= 1;
				}
			}
		}

		if ( bestDecodedBinIDs == nullptr )
			break;

		// Use projected color as new line dir

		colorDir = Vec3_Zero();
		for ( u32 colorID = 0; colorID < 16; ++colorID )
		{
			const u8 binID = bestDecodedBinIDs[colorID];
			colorDir += centredColors[colorID] * binIndexes[binID];
		}
		colorDir.Normalize();
	}

	if ( minSumDistanceSQ == FLT_MAX )
		return ImageBlock_EncodeBC1_Build( block, pixels, lineStride, sortedPixels[0], sortedPixels[lastUniquePixelID], binCount );

#if MAIN_COMPRESSOR_DEBUG == 1
	if ( s_plot )
	{
		const Vec3 p0 = bestEndColor0 - colorCenter;
		const Vec3 p1 = bestEndColor1 - colorCenter;

		Plot_NewObject( s_plot, Color_Make( 0, 0, 255, 255 ) );
		Plot_AddLine( s_plot, &p0, &p1 );
	}
#endif // #if MAIN_COMPRESSOR_DEBUG == 1

	u32 error;
	switch ( radius )
	{
	case 1:
		error = ImageBlock_EncodeBC1_Refine< 1 >( block, pixels, lineStride, &bestEndColor0, &bestEndColor1, binCount );
		break;
	case 2:
		error = ImageBlock_EncodeBC1_Refine< 2 >( block, pixels, lineStride, &bestEndColor0, &bestEndColor1, binCount );
		break;
	default:
		V6_ASSERT_NOT_SUPPORTED();
	}

#if MAIN_COMPRESSOR_DEBUG == 1
	if ( s_plot )
	{
		Color_s min = {};
		Color_s max = {};

		const u32 binCount = block->color0 > block->color1 ? 4 : 3;

		max.r = ((block->color0 >> 11) & 0x1F) << 3;
		max.g = ((block->color0 >> 5) & 0x3F) << 2;
		max.b = ((block->color0 >> 0) & 0x1F) << 3;

		min.r = ((block->color1 >> 11) & 0x1F) << 3;
		min.g = ((block->color1 >> 5) & 0x3F) << 2;
		min.b = ((block->color1 >> 0) & 0x1F) << 3;

		Vec3 endColor0, endColor1;
		EndColor_U8ToF32( &endColor0, &min );
		EndColor_U8ToF32( &endColor1, &max );

		const Vec3 p0 = endColor0 - colorCenter;
		const Vec3 p1 = endColor1 - colorCenter;

		Plot_NewObject( s_plot, Color_Make( 0, 255, 0, 255 ) );
		Plot_AddLine( s_plot, &p0, &p1 );
	}
#endif // #if MAIN_COMPRESSOR_DEBUG == 1

	return error;
}

static u32 ImageBlock_EncodeBC1_EmulateBlock( ImageBlock_s* block, const Color_s* pixels, u32 lineStride )
{
	u32 cellRGBA[16];
	u32 colorID = 0;
	for ( u32 y = 0; y < 4; ++y )
	{
		const u32 xOffset = y * lineStride;
		for ( u32 x = 0; x < 4; ++x, ++colorID )
		{
			const Color_s* pixel = &pixels[xOffset + x];
			cellRGBA[colorID] = (pixel->r << 24) | (pixel->g << 16) | (pixel->b << 8) | colorID;
		}
	}

	EncodedBlockEx_s encodedBlock;
	const u32 error = Block_Encode_Optimize( &encodedBlock, cellRGBA, 16 );

	block->color0 = encodedBlock.cellEndColors & 0xFFFF;
	block->color1 = encodedBlock.cellEndColors >> 16;
	block->bits = (u32)encodedBlock.cellColorIndices[0];

	return error;
}

static u32 ImageBlock_EncodeBC1_OptimizeFast( ImageBlock_s* block, const Color_s* pixels, u32 lineStride )
{
	// Compute centred colors

	Vec3 colorCenter = Vec3_Zero();
	Vec3 centredColors[16];

	{
		Vec3 colors[16];
		u32 colorID = 0;
		for ( u32 y = 0; y < 4; ++y )
		{
			const u32 xOffset = y * lineStride;
			for ( u32 x = 0; x < 4; ++x, ++colorID )
			{
				const Color_s* pixel = &pixels[xOffset + x];
				colors[colorID].x = pixel->r / 255.0f;
				colors[colorID].y = pixel->g / 255.0f;
				colors[colorID].z = pixel->b / 255.0f;
				colorCenter += colors[colorID];
			}
		}

		colorCenter *= 1.0f / 16.0f;
	
		for ( u32 colorID = 0; colorID < 16; ++colorID )
			centredColors[colorID] = colors[colorID] - colorCenter;
	}

	// Compute best line passing by centred colors

	Vec3 colorDir;

	{
		Mat3x3 covariance;
		Optimization_FindBestFittingLine3DPrecentred( &colorDir, &covariance, centredColors, 16 );

		if ( covariance.m_row0.x < FLT_EPSILON && covariance.m_row1.y < FLT_EPSILON && covariance.m_row2.z < FLT_EPSILON )
			return ImageBlock_EncodeBC1_BoundingBox( block, pixels, lineStride );
	}

	Vec3 bestEndColor0;
	Vec3 bestEndColor1;
	float minSumDistanceSQ = FLT_MAX;

	{
		// Project centred colors on line to find bounds

		float ts[16];
		float tMin = FLT_MAX;
		float tMax = -FLT_MAX;
		for ( u32 colorID = 0; colorID < 16; ++colorID )
		{
			ts[colorID] = Dot( centredColors[colorID], colorDir );
			tMin = Min( tMin, ts[colorID] );
			tMax = Max( tMax, ts[colorID] );
		}

		// Try multiple segments

		const float tLen = (tMax - tMin);
		const float tStep = tLen * 0.0125f;

		static const float colorEps = 3.0f * (2.0f / 255.f) * (2.0f / 255.0f);

		for ( int step0 = -4; step0 <= 4; ++step0 )
		{
			const float t0 = tMin + step0 * tStep;
			for ( int step1 = -4; step1 <= 4; ++step1 )
			{
				const float t1 = tMax + step1 * tStep;
				const float tDiff = t1 - t0;
				if ( tDiff < colorEps )
					continue;

				Vec3 endColor0 = colorCenter + t0 * colorDir;
				Vec3 endColor1 = colorCenter + t1 * colorDir;
				if ( endColor0.Max() < 0.0f || endColor0.Min() > 1.0f || endColor1.Max() < 0.0f || endColor1.Min() > 1.0f )
					continue;

				// Snap on the end color grid

				{
					Color_s encodedColor0;
					Color_s encodedColor1;
					EndColor_F32ToU8( &encodedColor0, &endColor0 );
					EndColor_F32ToU8( &encodedColor1, &endColor1 );
					EndColor_U8ToF32( &endColor0, &encodedColor0 );
					EndColor_U8ToF32( &endColor1, &encodedColor1 );
				}

				// Project centred colors on segment to find decoded colors

				float segmentSumDistanceSQ = 0.0f;

				Vec3 binColors[4];
				binColors[0] = endColor0 - colorCenter;
				binColors[3] = endColor1 - colorCenter;
				binColors[1] = Lerp( binColors[0], binColors[3], 1.0f / 3.0f );
				binColors[2] = Lerp( binColors[0], binColors[3], 2.0f / 3.0f );
				
				for ( u32 colorID = 0; colorID < 16; ++colorID )
				{
					float bestDistanceSQ = FLT_MAX;
					for ( u32 binID = 0; binID < 4; ++binID )
					{
						const float distanceSQ = (centredColors[colorID] - binColors[binID]).LengthSq();
						bestDistanceSQ = Min( bestDistanceSQ, distanceSQ );
					}
					
					segmentSumDistanceSQ += bestDistanceSQ;
				}

				// Keep the best segment

				if ( segmentSumDistanceSQ < minSumDistanceSQ )
				{
					minSumDistanceSQ = segmentSumDistanceSQ;
					bestEndColor0 = endColor0;
					bestEndColor1 = endColor1;
				}
			}
		}
	}

	if ( minSumDistanceSQ == FLT_MAX )
		return ImageBlock_EncodeBC1_BoundingBox( block, pixels, lineStride );

	return ImageBlock_EncodeBC1_Refine< 1 >( block, pixels, lineStride, &bestEndColor0, &bestEndColor1, 4 );
}

static void ImageBlock_DecodeBC1( Color_s* pixels, u32 lineStride, const ImageBlock_s* block )
{
	// Decode min/max

	Color_s min = {};
	Color_s max = {};

	const u32 binCount = block->color0 > block->color1 ? 4 : 3;

	max.r = ((block->color0 >> 11) & 0x1F) << 3;
	max.g = ((block->color0 >>  5) & 0x3F) << 2;
	max.b = ((block->color0 >>  0) & 0x1F) << 3;

	min.r = ((block->color1 >> 11) & 0x1F) << 3;
	min.g = ((block->color1 >>  5) & 0x3F) << 2;
	min.b = ((block->color1 >>  0) & 0x1F) << 3;
	
	// Make palette

	Color_s colors[4] = {};
	
	colors[0].r = max.r | (max.r >> 5);
	colors[0].g = max.g | (max.g >> 6);
	colors[0].b = max.b | (max.b >> 5);
	
	colors[1].r = min.r | (min.r >> 5);
	colors[1].g = min.g | (min.g >> 6);
	colors[1].b = min.b | (min.b >> 5);
	
	if ( binCount == 4 )
	{
		colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
		colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
		colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
		colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
		colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
		colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;
	}
	else
	{
		V6_ASSERT( binCount == 3 );
		colors[2].r = (colors[0].r + colors[1].r) >> 1;
		colors[2].g = (colors[0].g + colors[1].g) >> 1;
		colors[2].b = (colors[0].b + colors[1].b) >> 1;
	}

	// Decode bits

	u32 shift = 0;
	
	for ( u32 y = 0; y < 4; ++y )
	{
		const u32 xOffset = y * lineStride;
		for ( u32 x = 0; x < 4; ++x )
		{
			const u32 colorID = (block->bits >> shift) & 3;
			V6_ASSERT( colorID < binCount );
			pixels[xOffset + x] = colors[colorID];
			shift += 2;
		}
	}
}

static void TestImageCompression( const char* filenameSrc, IAllocator* allocator )
{
#if MAIN_COMPRESSOR_DEBUG == 1
	Plot_s plot;
	Plot_Create( &plot, "d:/tmp/plot/testImageCompression" );
	s_plot = nullptr;
#endif

	Image_s imageSrc = {};
	CFileReader fileReader;
	if ( !fileReader.Open( filenameSrc ) || !Image_ReadTga( &imageSrc, &fileReader, allocator ) )
	{
		V6_ERROR( "Unable to read %s\n", filenameSrc );
		return;
	}

	V6_ASSERT( IsPowOfTwo( imageSrc.width ) );
	V6_ASSERT( IsPowOfTwo( imageSrc.height ) );
	V6_MSG( "Compressing image %s %dx%d...\n", filenameSrc, imageSrc.width, imageSrc.height );

	Image_s imageDst = {};
	Image_Create( &imageDst, allocator, imageSrc.width, imageSrc.height );

	float maxMSE = 0.0f;
	float sumMSE = 0.0f;
	for ( u32 y = 0; y < imageSrc.height; y += 4 )
	{
		const u32 xOffset = y * imageSrc.width;
		for ( u32 x = 0; x < imageSrc.width; x += 4 )
		{
#if MAIN_COMPRESSOR_DEBUG == 1
			s_plot = x == 188 && y == (imageSrc.height - 4 - 188) ? &plot : nullptr;
#endif
			// ImageBlock_EncodeBC1_BoundingBox( &block, &imageSrc.pixels[xOffset + x], imageSrc.width );
			// ImageBlock_EncodeBC1_BruteForce( &block, &imageSrc.pixels[xOffset + x], imageSrc.width, 2 );
#if 0
			ImageBlock_s block3;
			ImageBlock_s block4;
			const u32 error3 = ImageBlock_EncodeBC1_Optimize( &block3, &imageSrc.pixels[xOffset + x], imageSrc.width, 3, 1 );
			const u32 error4 = error3 == 0 ? UINT_MAX : ImageBlock_EncodeBC1_Optimize( &block4, &imageSrc.pixels[xOffset + x], imageSrc.width, 4, 1 );
			ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, error3 < error4 ? &block3 : &block4 );
#elif 0
			ImageBlock_s block4;
			const u32 error4 = ImageBlock_EncodeBC1_Optimize( &block4, &imageSrc.pixels[xOffset + x], imageSrc.width, 4, 1 );
			ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, &block4 );
#elif 0
			ImageBlock_s block4;
			const u32 error4 = ImageBlock_EncodeBC1_OptimizeFast( &block4, &imageSrc.pixels[xOffset + x], imageSrc.width );
			ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, &block4 );
#elif 1
			ImageBlock_s block4;
			const u32 error4 = ImageBlock_EncodeBC1_EmulateBlock( &block4, &imageSrc.pixels[xOffset + x], imageSrc.width );
			ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, &block4 );
#elif 0
			ImageBlock_s block3;
			const u32 error3 = ImageBlock_EncodeBC1_Optimize( &block3, &imageSrc.pixels[xOffset + x], imageSrc.width, 3, 1 );
			ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, &block3 );
#endif
			
			// ImageBlock_DecodeBC1( &imageDst.pixels[xOffset + x], imageDst.width, &block4 );
			const float blockMSE =  ImageBlock_Error( imageSrc.pixels, imageDst.pixels, imageDst.width );
			maxMSE = Max( maxMSE, blockMSE );
			sumMSE += blockMSE;
		}

		// V6_MSG( "Line %d/%d\n", y, imageSrc.height );
	}

	const float avgMSE = sumMSE / (imageSrc.width * imageSrc.height);
	V6_MSG( "MSE: avg %g, max %g\n", avgMSE, maxMSE );

	char filenameWithoutExt[256];
	FilePath_TrimExtension( filenameWithoutExt, sizeof( filenameWithoutExt ), filenameSrc );
	const char* filenameDst = String_Format( "%s_bc1.bmp", filenameWithoutExt );
	CFileWriter fileWriter;
	if ( !fileWriter.Open( filenameDst ) )
	{
		V6_ERROR( "Unable to write %s\n", filenameDst );
		return;
	}	
	
	Image_WriteBitmap( &imageDst, &fileWriter );

#if MAIN_COMPRESSOR_DEBUG == 1
	Plot_Release( &plot );
	s_plot = nullptr;
#endif
}

void BenchBlockCompression( EncodedBlockEx_s* sum, const RawBlock_s* blocks, u32 blockCount )
{
	for ( u32 blockID = 0; blockID < blockCount; ++blockID )
	{
		EncodedBlockEx_s encodedBlock;
		Block_Encode_Optimize( &encodedBlock, blocks[blockID].cellRGBA, blocks[blockID].cellCount );
		sum->cellEndColors += encodedBlock.cellEndColors;
		sum->cellPresence += encodedBlock.cellPresence;
		sum->cellColorIndices[0] += encodedBlock.cellColorIndices[0];
		sum->cellColorIndices[1] += encodedBlock.cellColorIndices[1];
	}

	V6_MSG( "%d blocks compressed\n", blockCount );
}

u32 LoadBlockForCompression( RawBlock_s** blocks, IAllocator* heap, IStack* stack, const char* filename )
{
	CFileReader fileReader;
	if ( !fileReader.Open( filename ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return 0;
	}

	ScopedStack scopedStack( stack );

	CodecRawFrameDesc_s desc;
	CodecRawFrameData_s data;

	if ( !Codec_ReadRawFrame( &fileReader, &desc, &data, stack ) )
	{
		V6_ERROR( "Unable to read %s.\n", filename );
		return 0;
	}

	u32 blockPosOffsets[CODEC_RAWFRAME_BUCKET_COUNT];
	u32 blockDataOffsets[CODEC_RAWFRAME_BUCKET_COUNT];

	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		const u32 cellCount = desc.blockCounts[bucket] * cellPerBucketCount;
		blockPosOffsets[bucket] = blockPosCount;
		blockDataOffsets[bucket] = blockDataCount;
		blockPosCount += desc.blockCounts[bucket];
		blockDataCount += cellCount;
	}

	*blocks = heap->newArray< RawBlock_s >( blockPosCount );
	memset( *blocks, 0, blockPosCount * sizeof( RawBlock_s ) );

	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		const u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank )
		{
			const u32 blockPosID = blockPosOffsets[bucket] + blockRank;

			RawBlock_s* block = &(*blocks)[blockPosID];

			const u32 blockDataID = blockDataOffsets[bucket] + blockRank * cellPerBucketCount;
			for ( u32 cellID = 0; cellID < cellPerBucketCount; ++cellID )
			{
				const u32 rgba = ((u32*)data.blockData)[blockDataID + cellID];
				const u32 cellPos = rgba & 0xFF;
				if ( cellPos != 0xFF )
				{
					block->cellRGBA[cellPos] = rgba;
					++block->cellCount;
				}
			}
		}
	}

	return blockPosCount;
}

void TestBlockCompression( IAllocator* allocator )
{
	static const u32 testCount = 4;

	for ( u32 testID = 0; testID < testCount; ++testID )
	{
		u32 cellCount = 0;

		u32 cellRGBA[64] = {};
		memset( cellRGBA, 0xFF, sizeof( cellRGBA ) );

		while ( cellCount < 64 )
		{
retry:
			const u32 cellPos = rand() & 0x3F;
			
			for ( u32 cellRank = 0; cellRank < cellCount; ++cellRank )
			{
				if ( (cellRGBA[cellRank] & 0xFF) == cellPos )
					goto retry;
			}

			cellRGBA[cellCount] = ((rand() & 0xFF) << 24) | ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | cellPos;
			++cellCount;
		}

		EncodedBlockEx_s encodedBlock;
		Block_Encode_Optimize( &encodedBlock, cellRGBA, cellCount );

		u32 decodedCellRGBA[64] = {};
		u32 decodedCellCount = 0;
		Block_Decode( decodedCellRGBA, &decodedCellCount, &encodedBlock );

		qsort( cellRGBA, cellCount, sizeof( *cellRGBA ), CompareRGBAByAlpha );
		qsort( decodedCellRGBA, cellCount, sizeof( *decodedCellRGBA ), CompareRGBAByAlpha );

		printf( "%d cells:\n", cellCount );
		for ( u32 cellID = 0; cellID < cellCount; ++cellID )
		{
			const u32 color1R  = (cellRGBA[cellID] >> 24) & 0xFF;
			const u32 color1G  = (cellRGBA[cellID] >> 16) & 0xFF;
			const u32 color1B  = (cellRGBA[cellID] >>  8) & 0xFF;
			const u32 cellpos1 = cellRGBA[cellID] & 0xFF;

			const u32 color2R  = (decodedCellRGBA[cellID] >> 24) & 0xFF;
			const u32 color2G  = (decodedCellRGBA[cellID] >> 16) & 0xFF;
			const u32 color2B  = (decodedCellRGBA[cellID] >>  8) & 0xFF;
			const u32 cellpos2 = decodedCellRGBA[cellID] & 0xFF;

			V6_ASSERT( cellpos1 == cellpos2 );
			printf( "%02X %02X %02X %02X => %02X %02X %02X %02X\n", 
				color1R, color1G, color1B, cellpos1,
				color2R, color2G, color2B, cellpos2 );
		}
	}
}

static void TestImageCompressions( Stack* stack )
{
	//const char* filenameSrc = "D:/media/image/femme.tga";
	//const char* filenameSrcs[] = { "D:/media/image/montagne.tga" };
	//const char* filenameSrc = "D:/media/image/ville.tga";
	//const char* filenameSrcs[] = { "D:/media/image/plage.tga" };
	//const char* filenameSrc = "D:/media/image/rgb.tga";
	//const char* filenameSrcs[] = { "D:/media/image/sponza01.tga" };
	const char* filenameSrcs[] = { "D:/media/image/sponza_512_v6.tga" };
	//const char* filenameSrcs[] = { "D:/media/image/sponza_1024_ss0.tga", "D:/media/image/sponza_1024_ss1.tga", "D:/media/image/sponza_1024_ss2.tga" };

	const u32 fileCount = sizeof( filenameSrcs ) / sizeof( filenameSrcs[0] );

	for ( u32 fileID = 0; fileID < fileCount; ++fileID )
	{
		V6_PRINT( "\n" );
		TestImageCompression( filenameSrcs[fileID], stack );
	}
}

static void TextBestLineFitting( Stack* stack )
{
	static const u32 testCount = 128;

	V6_MSG( "TextBestLineFitting: started x%d\n", testCount );

	Plot_s plot;
	Plot_Create( &plot, "d:/tmp/plot/bestLineFitting" );

	for ( u32 testID = 0; testID < testCount; ++testID )
	{
		const u32 pointCount = 32;
		Vec3 dir = RandSphere();

		Vec3 points[pointCount];
		for ( u32 pointID = 0; pointID < pointCount; ++pointID )
			points[pointID] = 10.0f * RandFloat() * dir + 2.0f * RandFloat() * RandSphere();

		Vec3 bestOrg, bestDir;
		Optimization_FindBestFittingLine3D( &bestOrg, &bestDir, nullptr, points, pointCount );

		const float angle = Abs( Dot( dir, bestDir ) );
		if ( angle < 0.99f )
		{
			Vec3 p0 = bestOrg - 10.0f * bestDir;
			Vec3 p1 = bestOrg + 10.0f * bestDir;
			Plot_AddLine( &plot, &p0, &p1 );

			for ( u32 pointID = 0; pointID < pointCount; ++pointID )
				Plot_AddPoint( &plot, &points[pointID] );

			V6_MSG( "Angle %.5f: random ( %g %g %g ), approx ( %g %g %g )\n", angle, dir.x, dir.y, dir.z, bestDir.x, bestDir.y, bestDir.z );
		}
	}

	V6_MSG( "TextBestLineFitting: done\n" );

	Plot_Release( &plot );
}

END_V6_NAMESPACE

int main()
{
	V6_MSG( "Compressor 0.0\n" );

	v6::CHeap heap;

	v6::RawBlock_s* blocks = nullptr;

	{
		v6::Stack stack( &heap, 500 * 1024 * 1024 );

		const v6::u32 blockCount = v6::LoadBlockForCompression( &blocks, &heap, &stack, "D:/tmp/v6/ue_000000.v6f" );
		V6_MSG( "Loaded %d blocks\n", blockCount );
		if ( blockCount == 0 )
			return 1;
		
		const v6::u64 startTick = v6::GetTickCount();

		// v6::TextBestLineFitting( &stack );
		// v6::TestImageCompressions( &stack );
		// v6::TestBlockCompression( &stack );

		v6::EncodedBlockEx_s encodedBlockSum = {};
		v6::u32 testBlockCount = v6::Min( blockCount, 1000000u );
		v6::BenchBlockCompression( &encodedBlockSum, blocks, testBlockCount );

		const v6::u64 endTick = v6::GetTickCount();

		V6_MSG( "%.1fus/block\n", v6::ConvertTicksToSeconds( endTick - startTick ) * 1000000.0f / testBlockCount );

		V6_MSG( "\n" );
		V6_MSG( "%x\n", encodedBlockSum.cellEndColors );
		V6_MSG( "%llx\n", encodedBlockSum.cellPresence );
		V6_MSG( "%llx\n", encodedBlockSum.cellColorIndices[0] );
		V6_MSG( "%llx\n", encodedBlockSum.cellColorIndices[1] );
	}

	heap.free( blocks );

	return 0;
}
