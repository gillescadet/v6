/*V6*/

#include <v6/core/common.h>
#include <v6/core/filesystem.h>
#include <v6/core/memory.h>

BEGIN_V6_NAMESPACE

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

bool ConvertBinaryFileToHeaderFile( const char* binaryFile, IStack* stack )
{
	ScopedStack scopedStack( stack );

	u8* binaryData; 
	const int binarySize = FileSystem_ReadFile( binaryFile, (void**)&binaryData, stack );

	if ( binarySize == -1 )
	{
		V6_ERROR( "Unable to read %s\n", binaryFile );
		return false;
	}

	V6_MSG( "// generated from %s\n", binaryFile );
	V6_MSG( "const u8 g_binaryData[] = \n");
	V6_MSG( "{\n");

	const u32 columnCount = 16;
	u32 pos = 0;
	for ( ; pos < (u32)binarySize; pos += columnCount  )
	{
		V6_MSG( "\t" );
		for ( u32 offset = 0; offset < columnCount; ++offset )
			V6_MSG( "0x%02X, ", binaryData[pos + offset] );
		V6_MSG( "\n" );
	}

	if ( pos < (u32)binarySize )
	{
		V6_MSG( "\t" );
		for ( ; pos < (u32)binarySize; ++pos )
			V6_MSG( "0x%02X, ", binaryData[pos] );
		V6_MSG( "\n" );
	}

	V6_MSG( "};\n");

	return true;
}

END_V6_NAMESPACE

int main( int argc, char* argv[] )
{
	v6::CHeap heap;
	v6::Stack stack( &heap, 100 * 1024 * 1024 );

	if ( argc < 2 )
	{
		V6_MSG( "Usage: bin2h FILE\n" );
		return 1;
	}

	return v6::ConvertBinaryFileToHeaderFile( argv[1], &stack ) ? 0 : 1;
}