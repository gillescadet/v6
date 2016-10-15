/*V6*/

#include <v6/core/common.h>

#include <v6/codec/decoder.h>
#include <v6/codec/encoder.h>
#include <v6/core/memory.h>
#include <v6/core/time.h>

#define VALIDATE (ENCODER_STRICT_CELL)

BEGIN_V6_NAMESPACE

struct CommandArgs
{
	const char*		streamFilename;
	const char*		templateFilename;
	const char*		key;
	const char*		value;
	u32				frameOffset;
	u32				frameCount;
	u32				playRate;
	u32				compressionQuality;
	bool			extend;
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

#if 0
	static FILE* f = nullptr;
	if ( f == nullptr )
		fopen_s( &f, "d:/tmp/v6/encoder_log.txt", "wt" );
	fprintf( f, buffer );
#endif
}

//----------------------------------------------------------------------------------------------------

static void CommandArgs_PrintUsage()
{
	V6_MSG( "USAGE1: encoder -s STREAM_FILENAME -t RAW_FILENAME_TEMPLATE [-o FRAME_OFFSET] [-c FRAME_COUNT] [-r PLAY_RATE] [-q COMPRESSION_QUALITY] [-e]\n");
	V6_MSG( "\n");
	V6_MSG( " -s STREAM_FILENAME:       Stream filename.\n");
	V6_MSG( " -t RAW_FILENAME_TEMPLATE: Raw filename template. Use a printf like format to describe the raw filename with a variable frame ID.\n");
	V6_MSG( " -o FRAME_OFFSET:          Index of the first frame ID.\n");
	V6_MSG( " -c FRAME_COUNT:           Number of frames to encode.\n");
	V6_MSG( " -r PLAY_RATE:             Number of frames per second.\n");
	V6_MSG( " -q COMPRESSION_QUALITY:   Compresion quality. 0: low, 1: high.\n");
	V6_MSG( " -e:                       Extend (or create if it is not yet existing) the stream file.\n");
	V6_MSG( "\n");
	V6_MSG( "\n");

	V6_MSG( "USAGE2: encoder -s STREAM_FILENAME -k KEY_NAME [-v KEY_VALUE]\n");
	V6_MSG( "\n");
	V6_MSG( " -s STREAM_FILENAME:       Stream filename.\n");
	V6_MSG( " -k KEY_NAME:				Name of the key.\n");
	V6_MSG( " -v VALUE_NAME:			Value of the key to update.\n");
	V6_MSG( "\n");
}

static void CommandArgs_Init( CommandArgs* commandArgs )
{
	memset( commandArgs, 0, sizeof( *commandArgs ) );
	commandArgs->frameCount = 1;
	commandArgs->playRate = 75;

#if 0
	commandArgs->streamFilename = "D:/media/obj/crytek-sponza/sponza.v6";
	commandArgs->templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->playRate = 75;
	commandArgs->extend = false;
#endif

#if 0
	commandArgs->streamFilename = "D:/media/obj/default/default.v6";
	commandArgs->templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->playRate = 75;
	commandArgs->extend = false;
#endif

#if 0
	commandArgs->streamFilename = "D:/tmp/v6/ue.v6";
	commandArgs->templateFilename = "D:/tmp/v6/ue_%06d.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 2;
	commandArgs->playRate = 75;
	commandArgs->compressionQuality = 1;
	commandArgs->extend = false;
#endif

#if 0
	commandArgs->extend = true;
#endif

#if 0
	commandArgs->streamFilename = "D:/media/obj/default/default.v6";
	commandArgs->templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->key = "title";
	commandArgs->value = "Les vacances a la plage a St-Tropez";
#endif

#if 0
	commandArgs->streamFilename = "D:/media/obj/default/default.v6";
	commandArgs->templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->key = "title2";
	commandArgs->value = "Les vacances a la plage a Maurice";
#endif

#if 0
	commandArgs->streamFilename = "D:/media/obj/default/default.v6";
	commandArgs->templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->key = "title3";
	commandArgs->value = "Les vacances a la plage a Paris";
#endif

#if 0
	commandArgs->streamFilename = "D:/media/obj/default/default.v6";
	commandArgs->templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->key = "icon";
	commandArgs->value = "D:/media/image/test.tga";
#endif

#if 0
	commandArgs->streamFilename = "D:/media/obj/default/default.v6";
	commandArgs->templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->key = "title";
#endif
}

static bool CommandArgs_Read( CommandArgs* commandArgs, int argc, const char** argv )
{
	for ( u32 argID = 1; argID < (u32)argc; ++argID )
	{
		const bool isLast = argID+1 == argc;
		if ( argv[argID][0] == '-' )
		{
			if ( argv[argID][1] == 0 )
			{
				V6_ERROR( "Missing command letter following character '-'.\n" );
				return false;
			}
			if ( argv[argID][2] != 0 )
			{
				V6_ERROR( "Extra command letter in %s.\n", argv[argID] );
				return false;
			}
			switch( argv[argID][1] )
			{
			case 'c':
				if ( isLast )
				{
					V6_ERROR( "Command -o should be followed by the frame count.\n" );
					return false;
				}
				commandArgs->frameCount = atoi( argv[argID+1] );
				++argID;
				break;
			case 'e':
				commandArgs->extend = true;
				break;
			case 'k':
				if ( isLast )
				{
					V6_ERROR( "Command -k should be followed by the key.\n" );
					return false;
				}
				commandArgs->key = argv[argID+1];
				++argID;
				break;
			case 'o':
				if ( isLast )
				{
					V6_ERROR( "Command -o should be followed by the frame offset.\n" );
					return false;
				}
				commandArgs->frameOffset = atoi( argv[argID+1] );
				++argID;
				break;
			case 'q':
				if ( isLast )
				{
					V6_ERROR( "Command -q should be followed by the compression quality.\n" );
					return false;
				}
				commandArgs->compressionQuality = atoi( argv[argID+1] );
				++argID;
				break;
			case 'r':
				if ( isLast )
				{
					V6_ERROR( "Command -o should be followed by the play rate.\n" );
					return false;
				}
				commandArgs->playRate = atoi( argv[argID+1] );
				++argID;
				break;
			case 's':
				if ( isLast )
				{
					V6_ERROR( "Command -s should be followed by the stream filename.\n" );
					return false;
				}
				commandArgs->streamFilename = argv[argID+1];
				++argID;
				break;
			case 't':
				if ( isLast )
				{
					V6_ERROR( "Command -t should be followed by the raw filename template.\n" );
					return false;
				}
				commandArgs->templateFilename = argv[argID+1];
				++argID;
				break;
			case 'v':
				if ( isLast )
				{
					V6_ERROR( "Command -v should be followed by the value.\n" );
					return false;
				}
				commandArgs->value = argv[argID+1];
				++argID;
				break;
			default:
				V6_ERROR( "Command %s is not supported.\n", argv[argID] );
				return false;
			}
		}
	}

	if ( commandArgs->streamFilename == nullptr )
	{
		V6_ERROR( "Missing -s command to specify a stream filename.\n" );
		return false;
	}

	if ( commandArgs->key != nullptr )
	{
		V6_MSG( "Stream filename: %s\n", commandArgs->streamFilename );
		V6_MSG( "Key: %s\n", commandArgs->key );
		if ( commandArgs->value )
			V6_MSG( "Value: %s\n", commandArgs->value );
		return true;
	}

	if ( commandArgs->templateFilename == nullptr )
	{
		V6_ERROR( "Missing -t command to specify a raw filename template.\n" );
		return false;
	}

	if ( commandArgs->frameCount == 0 )
	{
		V6_ERROR( "Frame count should be greater than 0.\n", commandArgs->frameCount );
		return false;
	}

	if ( commandArgs->playRate != 75 )
	{
		V6_ERROR( "Playrate of %d is not supported.\n", commandArgs->playRate );
		return false;
	}

	if ( commandArgs->compressionQuality > 1 )
	{
		V6_ERROR( "Compression quality of %d is not supported.\n", commandArgs->compressionQuality );
		return false;
	}

	V6_MSG( "Stream filename: %s\n", commandArgs->streamFilename );
	V6_MSG( "Raw filename template: %s\n", commandArgs->templateFilename );
	V6_MSG( "Frame offset: %d\n", commandArgs->frameOffset );
	V6_MSG( "Frame count: %d\n", commandArgs->frameCount );
	V6_MSG( "Play rate: %d\n", commandArgs->playRate );
	V6_MSG( "Compression quality: %d\n", commandArgs->compressionQuality );
	V6_MSG( "Extend: %s\n", commandArgs->extend ? "yes" : "no" );

	return true;
}

int Main_HandleKey( const CommandArgs* commandArgs, IAllocator* heap )
{
	Stack stack( heap, MulMB( 1 ) );

	if ( commandArgs->value )
	{
		if ( !VideoStream_SetKeyValue( commandArgs->streamFilename, commandArgs->key, (u8*)commandArgs->value, (u32)strlen( commandArgs->value ) + 1u, &stack ) )
			return 1;;
	}
	else
	{
		CodecStreamDesc_s streamDesc;
		CodecStreamData_s streamData;

		if ( !VideoStream_LoadDescAndData( commandArgs->streamFilename, &streamDesc, &streamData, &stack ) )
			return 1;

		u32 valueSize;
		u8* value = VideoStream_GetKeyValue( &valueSize, &streamDesc, &streamData, commandArgs->key, &stack );
		if ( !value )
			return 1;

		V6_MSG( "Value: %s\n", value );
	}

	return 0;
}

int Main_HandleSequence( const CommandArgs* commandArgs, IAllocator* heap )
{
	u64 startTick = GetTickCount();

	if ( !VideoStream_Encode( commandArgs->streamFilename, commandArgs->templateFilename, commandArgs->frameOffset, commandArgs->frameCount, commandArgs->playRate, commandArgs->compressionQuality, commandArgs->extend, heap ) )
		return 1;

	const u64 endTick = GetTickCount();

	V6_MSG( "Duration: %5.3fs\n", ConvertTicksToSeconds( endTick - startTick ) );

#if VALIDATE

#pragma message( "### ENCODER VALIDATION ENABLED ###" )

	V6_MSG( "Validating...\n" );

	VideoStream_s videoStream = {};
	{
		Stack stack( heap, MulMB( 50 ) );
		if ( !VideoStream_Load( &videoStream, commandArgs->streamFilename, heap, &stack ) )
			return 1;
	}

	const bool validated = VideoStream_Validate( &videoStream, commandArgs->templateFilename, commandArgs->frameOffset, heap );

	VideoStream_Release( &videoStream, heap );

	if ( !validated )
	{
		V6_ERROR( "Invalid stream!\n" );
		return 1;
	}
	V6_MSG( "Well formed stream!\n" );
#endif // #if VALIDATE

	return 0;
}

//----------------------------------------------------------------------------------------------------

END_V6_NAMESPACE

int main( int argc, const char** argv )
{
	V6_MSG( "Encoder 0.0\n\n" );

	v6::CHeap heap;

	v6::CommandArgs commandArgs;
	v6::CommandArgs_Init( &commandArgs );
	if ( !v6::CommandArgs_Read( &commandArgs, argc, argv ) )
	{
		v6::CommandArgs_PrintUsage();
		return 1;
	}

	if ( commandArgs.key )
		return Main_HandleKey( &commandArgs, &heap );

	return Main_HandleSequence( &commandArgs, &heap );
}