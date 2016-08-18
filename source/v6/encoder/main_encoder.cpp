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
	u32				frameOffset;
	u32				frameCount;
	u32				playRate;
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
}

//----------------------------------------------------------------------------------------------------

static void CommandArgs_PrintUsage()
{
	V6_MSG( "USAGE: encoder -s STREAM_FILENAME -t RAW_FILENAME_TEMPLATE [-o FRAME_OFFSET] [-c FRAME_COUNT] [-r PLAY_RATE] [-e]\n");
	V6_MSG( "\n");
	V6_MSG( " -s STREAM_FILENAME:       Stream filename.\n");
	V6_MSG( " -t RAW_FILENAME_TEMPLATE: Raw filename template. Use a printf like format to describe the raw filename with a variable frame ID.\n");
	V6_MSG( " -o FRAME_OFFSET:          Index of the first frame ID.\n");
	V6_MSG( " -c FRAME_COUNT:           Number of frames to encode.\n");
	V6_MSG( " -r PLAY_RATE:             Number of frames per second.\n");
	V6_MSG( " -e:                       Extend (or create if it is not yet existing) the stream file.\n");
	V6_MSG( "\n");
}

static void CommandArgs_Init( CommandArgs* commandArgs )
{
	memset( commandArgs, 0, sizeof( *commandArgs ) );
	commandArgs->streamFilename = nullptr;
	commandArgs->templateFilename = nullptr;
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->playRate = 75;

#if 0
	commandArgs->streamFilename = "D:/media/obj/crytek-sponza/sponza.v6";
	commandArgs->templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->playRate = 1;
#endif

#if 0
	const char* streamFilename = "D:/media/obj/default/default.v6";
	const char* templateFilename = "D:/media/obj/default/default_%06d.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->playRate = 75;
#endif

#if 1
	commandArgs->streamFilename = "D:/tmp/v6/ue_1024.v6";
	commandArgs->templateFilename = "D:/tmp/v6/ue_%06d.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 2;
	commandArgs->playRate = 75;
	commandArgs->extend = true;
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
			case 'o':
				if ( isLast )
				{
					V6_ERROR( "Command -o should be followed by the frame offset.\n" );
					return false;
				}
				commandArgs->frameOffset = atoi( argv[argID+1] );
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
					V6_ERROR( "Command -t should be followed by the raw filename template .\n" );
					return false;
				}
				commandArgs->templateFilename = argv[argID+1];
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

	V6_MSG( "Stream filename: %s\n", commandArgs->streamFilename );
	V6_MSG( "Raw filename template: %s\n", commandArgs->templateFilename );
	V6_MSG( "Frame offset: %d\n", commandArgs->frameOffset );
	V6_MSG( "Frame count: %d\n", commandArgs->frameCount );
	V6_MSG( "Play rate: %d\n", commandArgs->playRate );
	V6_MSG( "Extend: %s\n", commandArgs->extend ? "yes" : "no" );

	return true;
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

	const v6::u64 startTick = v6::GetTickCount();

	if ( !v6::VideoStream_Encode( commandArgs.streamFilename, commandArgs.templateFilename, commandArgs.frameOffset, commandArgs.frameCount, commandArgs.playRate, commandArgs.extend, &heap ) )
		return 1;

	const v6::u64 endTick = v6::GetTickCount();

	V6_MSG( "Duration: %5.3fs\n", v6::ConvertTicksToSeconds( endTick - startTick ) );

#if VALIDATE

#pragma message( "### ENCODER VALIDATION ENABLED ###" )

	V6_MSG( "Validating...\n" );

	v6::VideoStream_s videoStream = {};
	{
		v6::Stack stack( &heap, v6::MulMB( 50 ) );
		if ( !v6::VideoStream_Load( &videoStream, streamFilename, &heap, &stack ) )
			return 1;
	}

	const bool validated = v6::VideoStream_Validate( &videoStream, templateFilename, frameOffset, &heap );

	v6::VideoStream_Release( &videoStream, &heap );

	if ( !validated )
	{
		V6_ERROR( "Invalid stream!\n" );
		return 1;
	}
	V6_MSG( "Well formed stream!\n" );
#endif // #if VALIDATE

	return 0;
}