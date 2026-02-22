/*V6*/

#include <v6/core/common.h>

#include <v6/codec/codec.h>
#include <v6/codec/compression.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>

BEGIN_V6_NAMESPACE

//----------------------------------------------------------------------------------------------------

void OutputMessage( u32 msgType, const char * format, ... )
{
	char buffer[4096];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof( buffer ), format, args);
	va_end( args );

	fputs( buffer, stdout );
}

//----------------------------------------------------------------------------------------------------

static bool Image_CompressBC1( const char* filename, IStack* stack )
{
	ScopedStack scopedStack( stack );

	if ( !FilePath_HasExtension( filename, "tga" ) )
	{
		V6_ERROR( "%s must be a TGA file.\n", filename );
		return false;
	}

	void* tgaFile;
	const int tgaFileSize = FileSystem_ReadFile( filename, &tgaFile, stack );
	if ( tgaFileSize == -1 )
	{
		V6_ERROR( "Unable to load %s.\n", filename );
		return false;
	}

	CBufferReader bufferReader( tgaFile, ToX64( (u64)tgaFileSize ) );
	Image_s image;
	if ( !Image_ReadTga( &image, &bufferReader, stack ) )
	{
		V6_ERROR( "Unable to read %s.\n", filename );
		return false;
	}

	if ( image.width != image.height )
	{
		V6_ERROR( "%s must be square.\n", filename);
		return false;
	}

	u32 imageSize = 0;
	{
		u32 width = image.width;
		u32 mipCount = 0;
		do 
		{
			imageSize += ImageBC1_GetSizeFromDimension( width, width );
			++mipCount;
			width >>= 1;
		} while ( width >= 4 );
	}

	u8* imageData = (u8*)stack->alloc( imageSize, "ImageBC1" );
	u8* chunk = imageData;

	{
		Image_s imageUp;
		Image_s imageDown = image;

		ImageBlockBC1_s* blocks = (ImageBlockBC1_s*)chunk;
		u32 width = image.width;
		u32 mipCount = 0;
		do 
		{
			if ( mipCount > 0 )
			{
				Image_Create( &imageDown, stack, width, width );
				Image_DownScaleBy2( &imageDown, &imageUp );
			}

			ImageBC1_s imageBC1;
			ImageBC1_CreateWithData( &imageBC1, blocks, width, width );
			blocks += ImageBC1_GetBlockCountFromDimension( width, width );

			ImageBC1_Encode( &imageBC1, &imageDown );
			imageUp = imageDown;

			++mipCount;
			width >>= 1;
		} while ( width >= 4 );
	}

	{
		char filenameBC1[256];
		FilePath_ChangeExtension( filenameBC1, sizeof( filenameBC1 ), filename, "bc1" );

		CFileWriter fileWriter;

		if ( !fileWriter.Open( filenameBC1 ,0 ) )
		{
			V6_ERROR( "Unable to create %s.\n", filenameBC1 );
			return false;
		}

		fileWriter.Write( imageData, ToX64( imageSize ) );
	}

	return true;
}

//----------------------------------------------------------------------------------------------------

END_V6_NAMESPACE

int main( int argc, const char* argv[] )
{
	V6_MSG( "Texture Compressor 0.0\n\n" );

	v6::CHeap heap;
	v6::Stack stack( &heap, v6::MulMB( 100 ) );

	if ( argc < 2 )
	{
		V6_MSG( "Usage: texture_compressor TGA_FILE\n" );
		return 1;
	}

	if ( !v6::Image_CompressBC1( argv[1], &stack ) )
		return 1;

	V6_MSG( "Done.\n" );
	return 0; 
}
