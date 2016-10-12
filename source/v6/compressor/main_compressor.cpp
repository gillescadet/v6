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

#define TEST_BEST_LINE	0
#define TEST_IMAGE		0
#define TEST_DOWNSCALE	1
#define TEST_BLOCK		0
#define BENCH_BLOCK		0

BEGIN_V6_NAMESPACE

struct RawBlock_s
{
	u32 cellRGBA[CODEC_CELL_MAX_COUNT];
	u32 cellCount;
};

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

static void TestImageCompression( const char* filenameSrc, IAllocator* allocator )
{
	Image_s imageSrc = {};
	CFileReader fileReader;
	if ( !fileReader.Open( filenameSrc, 0 ) || !Image_ReadTga( &imageSrc, &fileReader, allocator ) )
	{
		V6_ERROR( "Unable to read %s\n", filenameSrc );
		return;
	}

	V6_ASSERT( IsPowOfTwo( imageSrc.width ) );
	V6_ASSERT( IsPowOfTwo( imageSrc.height ) );
	V6_MSG( "Compressing image %s %dx%d...\n", filenameSrc, imageSrc.width, imageSrc.height );

	Image_s imageDst = {};
	Image_Create( &imageDst, allocator, imageSrc.width, imageSrc.height );

	u32 sumError = 0;
	u32 maxError = 0;
	for ( u32 y = 0; y < imageSrc.height; y += 4 )
	{
		const u32 xOffset = y * imageSrc.width;
		for ( u32 x = 0; x < imageSrc.width; x += 4 )
		{
			ImageBlockBC1_s block;
			const u32 error = ImageBlock_Encode_BC1( &block, &imageSrc.pixels[xOffset + x], imageSrc.width );
			ImageBlock_Decode_BC1( &imageDst.pixels[xOffset + x], imageDst.width, &block );
			sumError += error;
			maxError = Max( maxError, error );
		}

		V6_MSG( "Line %d/%d\n", y, imageSrc.height );
	}

	const float avgError = (float)sumError / (imageSrc.width * imageSrc.height);
	V6_MSG( "Error: avg %g, max %d\n", avgError, maxError );

	char filenameWithoutExt[256];
	FilePath_TrimExtension( filenameWithoutExt, sizeof( filenameWithoutExt ), filenameSrc );
	const char* filenameDst = String_Format( "%s_bc1.bmp", filenameWithoutExt );
	CFileWriter fileWriter;
	if ( !fileWriter.Open( filenameDst, false ) )
	{
		V6_ERROR( "Unable to write %s\n", filenameDst );
		return;
	}	
	
	Image_WriteBitmap( &imageDst, &fileWriter );
}

void BenchBlockCompression( EncodedBlockEx_s* sum, const RawBlock_s* blocks, u32 blockCount )
{
	for ( u32 blockID = 0; blockID < blockCount; ++blockID )
	{
		EncodedBlockEx_s encodedBlock;
		Block_Encode_Optimize( &encodedBlock, blocks[blockID].cellRGBA, blocks[blockID].cellCount, 1 );
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
	if ( !fileReader.Open( filename, FILE_OPEN_FLAG_UNBUFFERED ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return 0;
	}

	ScopedStack scopedStack( stack );

	CodecRawFrameDesc_s desc;
	CodecRawFrameData_s data;

	if ( !Codec_ReadRawFrame( &fileReader, &desc, &data, nullptr, stack ) )
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

	u32 rawBlockCount = 0;
	for ( u32 bucket = 0; bucket < CODEC_RAWFRAME_BUCKET_COUNT; ++bucket )
	{
		if ( bucket != 2 )
			continue;

		const u32 cellPerBucketCount = 1 << (bucket + 2);

		for ( u32 blockRank = 0; blockRank < desc.blockCounts[bucket]; ++blockRank )
		{
			const u32 blockPosID = blockPosOffsets[bucket] + blockRank;

			RawBlock_s* block = &(*blocks)[rawBlockCount];

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

			++rawBlockCount;
		}
	}

	return rawBlockCount;
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
		Block_Encode_Optimize( &encodedBlock, cellRGBA, cellCount, 1 );

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
	//const char* filenameSrcs[] = { "D:/media/image/sponza_512_v6.tga" };
	//const char* filenameSrcs[] = { "D:/media/image/sponza_1024_ss0.tga", "D:/media/image/sponza_1024_ss1.tga", "D:/media/image/sponza_1024_ss2.tga" };
	//const char* filenameSrcs[] = { "D:/media/image/ue_000000_S00_F1.tga" };
	const char* filenameSrcs[] = { "D:/media/image/test.tga" };

	const u32 fileCount = sizeof( filenameSrcs ) / sizeof( filenameSrcs[0] );

	for ( u32 fileID = 0; fileID < fileCount; ++fileID )
	{
		V6_PRINT( "\n" );
		TestImageCompression( filenameSrcs[fileID], stack );
	}
}

static void TestImageDownscale( const char* filenameSrc, IAllocator* allocator )
{
	Image_s imageUp = {};
	CFileReader fileReader;
	if ( !fileReader.Open( filenameSrc, 0 ) || !Image_ReadTga( &imageUp, &fileReader, allocator ) )
	{
		V6_ERROR( "Unable to read %s\n", filenameSrc );
		return;
	}

	V6_ASSERT( IsPowOfTwo( imageUp.width ) );
	V6_ASSERT( IsPowOfTwo( imageUp.height ) );
	V6_MSG( "Downscaling image %s %dx%d...\n", filenameSrc, imageUp.width, imageUp.height );

	Image_s imageDown = {};
	Image_Create( &imageDown, allocator, imageUp.width >> 1, imageUp.height >> 1 );

	Image_DownScaleBy2( &imageDown, &imageUp );

	V6_MSG( "Downscaled to %dx%d\n", imageDown.width, imageDown.height );

	char filenameWithoutExt[256];
	FilePath_TrimExtension( filenameWithoutExt, sizeof( filenameWithoutExt ), filenameSrc );
	const char* filenameDst = String_Format( "%s_downscaled.bmp", filenameWithoutExt );
	CFileWriter fileWriter;
	if ( !fileWriter.Open( filenameDst, false ) )
	{
		V6_ERROR( "Unable to write %s\n", filenameDst );
		return;
	}	
	
	Image_WriteBitmap( &imageDown, &fileWriter );
}

static void TestImageDownscales( Stack* stack )
{
	const char* filenameSrcs[] = { "D:/media/image/test.tga" };

	const u32 fileCount = sizeof( filenameSrcs ) / sizeof( filenameSrcs[0] );

	for ( u32 fileID = 0; fileID < fileCount; ++fileID )
	{
		V6_PRINT( "\n" );
		TestImageDownscale( filenameSrcs[fileID], stack );
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

#if BENCH_BLOCK == 1
		const v6::u32 blockCount = v6::LoadBlockForCompression( &blocks, &heap, &stack, "D:/tmp/v6/ue_000000.v6f" );
		V6_MSG( "Loaded %d blocks\n", blockCount );
		if ( blockCount == 0 )
			return 1;
#endif

		const v6::u64 startTick = v6::GetTickCount();

#if TEST_BEST_LINE == 1
		v6::TextBestLineFitting( &stack );
#endif

#if TEST_IMAGE == 1
		v6::TestImageCompressions( &stack );
		v6::u32 itemCount = 1;
#endif

#if TEST_DOWNSCALE == 1
		v6::TestImageDownscales( &stack );
		v6::u32 itemCount = 1;
#endif

#if TEST_BLOCK == 1
		v6::TestBlockCompression( &stack );
#endif

#if BENCH_BLOCK == 1
		v6::EncodedBlockEx_s encodedBlockSum = {};
		v6::u32 itemCount = v6::Min( blockCount, 100000u );
		v6::BenchBlockCompression( &encodedBlockSum, blocks, testBlockCount );
#endif

		const v6::u64 endTick = v6::GetTickCount();

		V6_MSG( "%.1fus/item\n", v6::ConvertTicksToSeconds( endTick - startTick ) * 1000000.0f / itemCount );

#if BENCH_BLOCK == 1
		V6_MSG( "\n" );
		V6_MSG( "%x\n", encodedBlockSum.cellEndColors );
		V6_MSG( "%llx\n", encodedBlockSum.cellPresence );
		V6_MSG( "%llx\n", encodedBlockSum.cellColorIndices[0] );
		V6_MSG( "%llx\n", encodedBlockSum.cellColorIndices[1] );
#endif
	}

	heap.free( blocks );

	return 0;
}
