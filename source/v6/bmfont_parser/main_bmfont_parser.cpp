/*V6*/

#include <v6/core/common.h>
#include <v6/core/filesystem.h>
#include <v6/core/memory.h>
#include <v6/graphic/font_data.h>

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

bool IsNewLine( char car )
{
	return car == '\n' || car == '\r';
}

bool IsSpace( char car )
{
	return car == ' ' || car == '\t';
}

bool IsAlphaNum( char car )
{
	return 
		(car >= 'a' && car <= 'z') ||
		(car >= 'A' && car <= 'Z') ||
		(car >= '0' && car <= '9') || car == '-' ||
		car == '_' || car == '~' || car == '=';
}

char* ReadLine( char** cursor )
{
	char *s = *cursor;

	while ( *s && IsNewLine( *s ) )
		++s;

	*cursor = s;

	if ( *s == 0 )
		return nullptr;

	while ( *s && !IsNewLine( *s ) )
		++s;

	if ( *s )
	{
		*s = 0;
		++s;
	}

	char* line = *cursor;
	*cursor = s;
	
	return line;
}

char* ReadToken( char** cursor, IAllocator* allocator )
{
	char *s = *cursor;

	while ( *s && IsSpace( *s ) )
		++s;

	char* token = s;

	while ( *s && IsAlphaNum( *s ) )
		++s;

	u32 len = (u32)(s-token);
	char* buffer = allocator->newArray< char >( len + 1, "BmFont" );
	strncpy_s( buffer, len+1, token, len );

	*cursor = s;

	return buffer;
}

void ExtractKeyValue( char** key, char** value, char* token )
{
	*key = token;

	char *s = token;

	while ( *s && !IsSpace( *s ) && *s != '=' )
		++s;

	if ( *s != '=' )
	{
		*value = "";
		return;
	}

	*s = 0;
	++s;

	*value = s;
}

bool ParseBMFont( const char* binaryFile, IStack* stack )
{
	ScopedStack scopedStack( stack );

	void* data;

	if ( FileSystem_ReadFile( binaryFile, &data, stack ) <= 0 )
	{
		V6_ERROR( "Unable to read %s\n", binaryFile );
		return false;
	}

	char* cursor = (char*)data;

	FontCharacter_s fontCharacters[256] = {};

	for (;;)
	{
		char* line = ReadLine( &cursor );
		if ( !line )
			break;

		char* token = ReadToken( &line, stack );

		if ( _stricmp( token, "char" ) != 0 )
			continue;

		u32 id = (u32)-1; 
		FontCharacter_s fontCharacter = {};

		for (;;)
		{
			token = ReadToken( &line, stack);
			if ( !token || !*token)
				break;

			char* key;
			char* value;
			ExtractKeyValue( &key, &value, token );

			if ( _stricmp( token, "id" ) == 0 )
				id = atoi( value );
			if ( _stricmp( token, "x" ) == 0 )
				fontCharacter.x = atoi( value );
			else if ( _stricmp( token, "y" ) == 0 )
				fontCharacter.y = atoi( value );
			else if ( _stricmp( token, "width" ) == 0 )
				fontCharacter.width = atoi( value );
			else if ( _stricmp( token, "height" ) == 0 )
				fontCharacter.height = atoi( value );
			else if ( _stricmp( token, "xoffset" ) == 0 )
				fontCharacter.xoffset = atoi( value );
			else if ( _stricmp( token, "yoffset" ) == 0 )
				fontCharacter.yoffset = atoi( value );
			else if ( _stricmp( token, "xadvance" ) == 0 )
				fontCharacter.xadvance = atoi( value );
		}

		if ( id >= 256 )
		{
			V6_MSG( "ID %d out of range\n", id );
			return false;
		}

		fontCharacters[id] = fontCharacter;
	}

	V6_MSG( "// generated from %s\n", binaryFile );
	V6_MSG( "const FontCharacter_s g_fontCharacters[] = \n");
	V6_MSG( "{\n");

	for ( u32 id = 0; id < 256; ++id )
	{
		const FontCharacter_s* fontCharacter = &fontCharacters[id];
		V6_MSG( "\t{ %3u, %3u, %2u, %2u, %2d, %2d, %2u },\n", 
			fontCharacter->x,
			fontCharacter->y,
			fontCharacter->width,
			fontCharacter->height,
			fontCharacter->xoffset,
			fontCharacter->yoffset,
			fontCharacter->xadvance );
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
		V6_MSG( "Usage: bmfont_parser FILE\n" );
		return 1;
	}

	return v6::ParseBMFont( argv[1], &stack ) ? 0 : 1;
}