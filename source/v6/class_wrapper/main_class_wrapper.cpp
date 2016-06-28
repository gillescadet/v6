/*V6*/

#include <v6/core/common.h>

#include <v6/core/filesystem.h>
#include <v6/core/memory.h>

#define TEMPLATE		static const char* const
#define TEXT(...)		#__VA_ARGS__

BEGIN_V6_NAMESPACE

//----------------------------------------------------------------------------------------------------

void OutputMessage( const char * format, ... )
{
  char buffer[4096];
  va_list args;
  va_start( args, format );
  vsprintf_s( buffer, sizeof( buffer ), format, args);
  va_end( args );

  printf( buffer );
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
		(car >= '0' && car <= '9') ||
		car == '_' || car == '~' ;
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
	char* buffer = allocator->newArray< char >( len + 1 );
	strncpy_s( buffer, len+1, token, len );

	*cursor = s;

	return buffer;
}

void ReadFunction( char** ret, char** function, char** args, char* signature )
{
	while ( *signature && IsSpace( *signature ) )
		++signature;

	*ret = signature;

	while ( *signature && *signature != '(' )
		++signature;

	if ( *signature != '(' )
	{
		V6_ERROR( "Malformed function\n" );
		exit( 1 );
	}

	char* fend = signature-1;
	*signature = 0;
	++signature;

	while ( fend > *ret && IsSpace( *fend ) )
		--fend;

	if ( fend == *ret )
	{
		V6_ERROR( "Malformed function\n" );
		exit( 1 );
	}

	while ( fend > *ret && IsAlphaNum( *fend ) )
		--fend;

	if ( *fend == '~' )
	{
		if ( fend != *ret )
		{
			V6_ERROR( "Malformed function\n" );
			exit( 1 );
		}

		*ret = "~";
		*function = fend;
	}
	else
	{
		if ( fend == *ret )
		{
			V6_ERROR( "Malformed function\n" );
			exit( 1 );
		}

		*fend = 0;
		*function = fend+1;
	}

	*args = signature;

	while ( *signature && *signature != ')' )
		++signature;

	if ( *signature != ')' )
	{
		V6_ERROR( "Malformed function\n" );
		exit( 1 );
	}

	*signature = 0;
}

u32 ReadArgs( char** types, char** params, u32 maxTypeCount, char* args )
{
	u32 typeCount = 0;

	for (;;)
	{
		if ( typeCount + 1 > maxTypeCount )
		{
			V6_ERROR( "Too much args\n" );
			exit( 1 );
		}

		while ( *args && IsSpace( *args ) )
			++args;

		if ( *args == 0 )
			break;

		char* type = args;

		while ( *args && *args != ',' )
			++args;

		char* pend = args-1;

		while ( pend > type && IsSpace( *pend ) )
			--pend;

		if ( pend == type )
		{
			V6_ERROR( "Malformed args\n" );
			exit( 1 );
		}

		while ( pend > type && IsAlphaNum( *pend ) )
			--pend;

		if ( pend == type )
		{
			V6_ERROR( "Malformed args\n" );
			exit( 1 );
		}

		*pend = 0;
		++pend;

		types[typeCount] = type;
		params[typeCount] = pend;
		++typeCount;

		if ( *args == 0 )
			break;

		*args = 0;
		++args;
	}

	return typeCount;
}

void Parse( const char* className, const char* filenameSrc, IAllocator* allocator )
{
	void* data;

	if ( FileSystem_ReadFile( filenameSrc, &data, allocator ) <= 0 )
		exit( 1 );

	char* cursor = (char*)data;

	V6_MSG( "class %sWrap : public %s\n", className, className );
	V6_MSG( "{\n" );
	V6_MSG( "public:\n" );
	V6_MSG( "\n" );

	for (;;)
	{
		char* line = ReadLine( &cursor );
		if ( !line )
			break;

		char* token = ReadToken( &line, allocator );

		if ( _stricmp( token, "virtual" ) != 0 )
			continue;
		
		char* ret;
		char* function;
		char* args;
		ReadFunction( &ret, &function, &args, line );

		if ( *ret == '~' )
			continue;

		char* types[16] = {};
		char* params[16] = {};
		const u32 typeCount = ReadArgs( types, params, 16, args );

		V6_MSG( "\tvirtual %s %s( ", ret, function );
		for ( u32 typeID = 0; typeID < typeCount; ++typeID )
			V6_MSG( typeID == 0 ? "%s %s" : ", %s %s", types[typeID], params[typeID] );
		V6_MSG( " ) final override\n" );
		V6_MSG( "\t{\n" );
		if ( _stricmp( ret, "void" ) == 0 )
			V6_MSG( "\t\tm_wrapped->%s( ", function );
		else
			V6_MSG( "\t\treturn m_wrapped->%s( ", function );
		for ( u32 typeID = 0; typeID < typeCount; ++typeID )
			V6_MSG( typeID == 0 ? "%s" : ", %s", params[typeID] );
		V6_MSG( " );\n" );
		V6_MSG( "\t}\n" );
		V6_MSG( "\t\n" );
	}

	V6_MSG( "public:\n" );
	V6_MSG( "\n" );
	V6_MSG( "\t%s* m_wrapped;\n", className );
	V6_MSG( "};\n" );
}

END_V6_NAMESPACE

int main()
{
	V6_MSG( "Class Wrapper 0.0\n" );

	v6::CHeap heap;
	v6::Stack stack( &heap, 100 * 1024 * 1024 );

	v6::Parse( "FDynamicRHI",			"../../source/v6/class_wrapper/FDynamicRHI.txt",			&stack );
	v6::Parse( "IRHICommandContext",	"../../source/v6/class_wrapper/IRHICommandContext.txt",		&stack );

	return 0;
}