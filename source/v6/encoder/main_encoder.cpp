/*V6*/

#include <v6/core/common.h>

#include <v6/codec/decoder.h>
#include <v6/codec/encoder.h>
#include <v6/core/filesystem.h>
#include <v6/core/memory.h>
#include <v6/core/time.h>

#define V6_VERSION_MAJOR		0
#define V6_VERSION_MINOR		1

#define VALIDATE (ENCODER_STRICT_CELL)

BEGIN_V6_NAMESPACE

struct CommandArgs
{
	const char*		inputFilenames[16];
	const char*		streamFilename;
	const char*		templateFilename;
	const char*		key;
	const char*		value;
	u32				frameOffset;
	u32				frameCount;
	u32				frameRate;
	u32				compressionQuality;
	u32				inputFilenameCount;
	u32				firstSequenceToRemoveCount;
	u32				lastSequenceToRemoveCount;
	bool			extend;
};

static FILE*	s_logFile = nullptr;
static Mutex_s	s_logMutex;

//----------------------------------------------------------------------------------------------------

void OutputMessage( u32 msgType, const char * format, ... )
{
	char buffer[4096];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof( buffer ), format, args);
	va_end( args );

	fputs( buffer, stdout );

	if ( s_logFile )
	{
		Mutex_Lock( &s_logMutex );
		switch( msgType )
		{
		case MSG_WARNING:
			fputs( "[WARNING] ", s_logFile );
			break;
		case MSG_ERROR:
			fputs( "[ERROR] ", s_logFile );
			break;
		case MSG_FATAL:
			fputs( "[FATAL] ", s_logFile );
			break;
		}
		fputs( buffer, s_logFile );
		Mutex_Unlock( &s_logMutex );
	}

	if ( msgType == MSG_FATAL )
		exit( 1 );
}

//----------------------------------------------------------------------------------------------------

static void LogFile_Create( const CommandArgs* commandArgs )
{
	char logFilename[256];
	v6::FilePath_ChangeExtension( logFilename, sizeof( logFilename ), commandArgs->streamFilename, "log" );
	fopen_s( &v6::s_logFile, logFilename, commandArgs->extend ? "a+t" : "wt" );
	if ( v6::s_logFile )
		Mutex_Create( &v6::s_logMutex );
	else
		V6_WARNING( "Unable to open log file %s\n", logFilename );
}

static void LogFile_Release()
{
	if ( v6::s_logFile )
	{
		Mutex_Lock( &v6::s_logMutex );
		FILE* const logFile = v6::s_logFile;
		v6::s_logFile = nullptr;
		Mutex_Unlock( &v6::s_logMutex );

		Mutex_Release( &v6::s_logMutex );
		fclose( logFile );
	}
}

static void CommandArgs_PrintUsage()
{
	V6_MSG( "*** Encode multiple raw frames ***\n");
	V6_MSG( "USAGE: encoder -s STREAM_FILENAME -t RAW_FILENAME_TEMPLATE [-o FRAME_OFFSET] [-c FRAME_COUNT] [-r PLAY_RATE] [-q COMPRESSION_QUALITY] [-e]\n");
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

	V6_MSG( "*** Add a key\\value pair ***\n");
	V6_MSG( "USAGE: encoder -s STREAM_FILENAME -k KEY_NAME [-v KEY_VALUE]\n");
	V6_MSG( "\n");
	V6_MSG( " -s STREAM_FILENAME:       Stream filename.\n");
	V6_MSG( " -k KEY_NAME:              Name of the key.\n");
	V6_MSG( " -v VALUE_NAME:            Value of the key to update.\n");
	V6_MSG( "\n");

	V6_MSG( "*** Merge multiple streams ***\n");
	V6_MSG( "USAGE: encoder -s STREAM_FILENAME -a INPUT_STREAM_FILENAME#1 -a INPUT_STREAM_FILENAME#2 ... -a INPUT_STREAM_FILENAME#N\n");
	V6_MSG( "\n");
	V6_MSG( " -s STREAM_FILENAME:       Stream filename.\n");
	V6_MSG( " -a INPUT_STREAM_FILENAME: Input stream filenames.\n");
	V6_MSG( "\n");

	V6_MSG( "*** Trim a stream ***\n");
	V6_MSG( "USAGE: encoder -s STREAM_FILENAME -i INPUT_STREAM_FILENAME -f FIRST_SEQUENCE_COUNT -l LAST_SEQUENCE_COUNT\n" );
	V6_MSG( "\n");
	V6_MSG( " -s STREAM_FILENAME:       Stream filename.\n");
	V6_MSG( " -a INPUT_STREAM_FILENAME: Input stream filename.\n");
	V6_MSG( " -f FIRST_SEQUENCE_COUNT:  Number of sequences to remove from the beginning.\n");
	V6_MSG( " -l LAST_SEQUENCE_COUNT:   Number of sequences to remove from the end.\n");
	V6_MSG( "\n");
}

static void CommandArgs_Init( CommandArgs* commandArgs )
{
	memset( commandArgs, 0, sizeof( *commandArgs ) );
	commandArgs->frameCount = 1;
	commandArgs->frameRate = Codec_GetDefaultFrameRate();

#if 0
	commandArgs->streamFilename = "d:/tmp/v6/ue.df";
	commandArgs->templateFilename = "d:/tmp/v6/ue_%06u.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->frameRate = 90;
	commandArgs->compressionQuality = 1;
	commandArgs->extend = false;
#endif

#if 0
	commandArgs->streamFilename = "d:/tmp/v6/fight_scene_movie.df";
	commandArgs->key = "icon";
	commandArgs->value = "d:/tmp/v6/fight_scene_movie.tga";
#endif

#if 0
	commandArgs->streamFilename = "v6/test.df";
	commandArgs->inputFilenames[0] = "d:/tmp/v6/Fight Scene 1400+.df";
	commandArgs->inputFilenames[1] = "d:/tmp/v6/Fight Scene 1400+.df";
	commandArgs->inputFilenameCount = 2;
#endif

#if 0
	commandArgs->streamFilename = "d:/tmp/v6/infiltrator_final_trimmed.df";
	commandArgs->inputFilenames[0] = "d:/tmp/v6/infiltrator_final.df";
	commandArgs->firstSequenceToRemoveCount = 38;
	commandArgs->lastSequenceToRemoveCount = 0;
#endif

#if 0
	commandArgs->streamFilename = "c:/tmp/v6/ue.df";
	commandArgs->templateFilename = "c:/tmp/v6/ue_%06u.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->frameRate = 90;
	commandArgs->compressionQuality = 1;
	commandArgs->extend = false;
#elif 0
	commandArgs->streamFilename = "c:/tmp/v6/ue.df";
	commandArgs->templateFilename = "c:/tmp/v6/ue_%06u.v6f";
	commandArgs->frameOffset = 1;
	commandArgs->frameCount = 1;
	commandArgs->frameRate = 90;
	commandArgs->compressionQuality = 1;
	commandArgs->extend = true;
#endif

#if 0
	commandArgs->streamFilename = "c:/tmp/v6/kite21.df";
	commandArgs->templateFilename = "c:/tmp/v6/kite21_%06u.v6f";
	commandArgs->frameOffset = 0;
	commandArgs->frameCount = 1;
	commandArgs->frameRate = 1;
	commandArgs->compressionQuality = 0;
	commandArgs->extend = false;
#endif
}

static void CommandArgs_Log( const CommandArgs* commandArgs )
{
	V6_MSG( "Stream filename: %s\n", commandArgs->streamFilename );

	if ( commandArgs->key != nullptr )
	{
		V6_MSG( "Key: %s\n", commandArgs->key );
		if ( commandArgs->value )
			V6_MSG( "Value: %s\n", commandArgs->value );
	}
	else if ( commandArgs->inputFilenameCount > 0 )
	{
		for ( u32 streamID = 0; streamID < commandArgs->inputFilenameCount; ++streamID )
			V6_MSG( "Input stream filename #%d: %s\n", streamID, commandArgs->inputFilenames[streamID] );
	}
	else if ( commandArgs->firstSequenceToRemoveCount > 0 || commandArgs->lastSequenceToRemoveCount > 0 )
	{
		V6_MSG( "Input stream filename: %s\n", commandArgs->inputFilenames[0] );
		V6_MSG( "First sequence to remove: %d\n", commandArgs->firstSequenceToRemoveCount );
		V6_MSG( "Last sequence to remove: %d\n", commandArgs->lastSequenceToRemoveCount );
	}
	else
	{
		V6_MSG( "Raw filename template: %s\n", commandArgs->templateFilename );
		V6_MSG( "Frame offset: %d\n", commandArgs->frameOffset );
		V6_MSG( "Frame count: %d\n", commandArgs->frameCount );
		V6_MSG( "Frame rate: %d\n", commandArgs->frameRate );
		V6_MSG( "Compression quality: %d\n", commandArgs->compressionQuality );
		V6_MSG( "Extend: %s\n", commandArgs->extend ? "yes" : "no" );
	}
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
			case 'a':
				if ( isLast )
				{
					V6_ERROR( "Command -a should be followed by the input stream filename.\n" );
					return false;
				}
				V6_ASSERT( commandArgs->inputFilenameCount < V6_ARRAY_COUNT( commandArgs->inputFilenames ) );
				commandArgs->inputFilenames[commandArgs->inputFilenameCount] = argv[argID+1];
				++argID;
				++commandArgs->inputFilenameCount;
				break;
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
			case 'f':
				if ( isLast )
				{
					V6_ERROR( "Command -f should be followed by the first sequence count.\n" );
					return false;
				}
				commandArgs->firstSequenceToRemoveCount = atoi( argv[argID+1] );
				++argID;
				break;
			case 'i':
				if ( isLast )
				{
					V6_ERROR( "Command -i should be followed by the input stream filename.\n" );
					return false;
				}
				V6_ASSERT( commandArgs->inputFilenameCount == 0 );
				commandArgs->inputFilenames[0] = argv[argID+1];
				++argID;
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
			case 'l':
				if ( isLast )
				{
					V6_ERROR( "Command -l should be followed by the last sequence count.\n" );
					return false;
				}
				commandArgs->lastSequenceToRemoveCount = atoi( argv[argID+1] );
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
					V6_ERROR( "Command -o should be followed by the frame rate.\n" );
					return false;
				}
				commandArgs->frameRate = atoi( argv[argID+1] );
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

	LogFile_Create( commandArgs );

	if ( commandArgs->key != nullptr )
		return true;

	if ( commandArgs->inputFilenameCount > 0 )
		return true;

	if ( commandArgs->firstSequenceToRemoveCount > 0 || commandArgs->lastSequenceToRemoveCount > 0 )
	{
		if ( commandArgs->inputFilenames[0] == nullptr )
		{
			V6_ERROR( "Missing -i command to specify an input stream filename.\n" );
			return false;
		}

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

	if ( !Codec_IsFrameRateSupported( commandArgs->frameRate ) )
	{
		V6_ERROR( "Frame rate of %d is not supported.\n", commandArgs->frameRate );
		return false;
	}

	if ( commandArgs->compressionQuality > 1 )
	{
		V6_ERROR( "Compression quality of %d is not supported.\n", commandArgs->compressionQuality );
		return false;
	}

	return true;
}

int Main_HandleKey( const CommandArgs* commandArgs, IAllocator* heap )
{
	Stack stack( heap, MulMB( 1 ) );

	if ( commandArgs->value )
	{
		if ( !VideoStream_SetKeyValue( commandArgs->streamFilename, commandArgs->key, (u8*)commandArgs->value, (u32)strlen( commandArgs->value ) + 1u, &stack ) )
			return 1;
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

int Main_HandleMerge( const CommandArgs* commandArgs, IAllocator* heap )
{
	return VideoStream_Merge( commandArgs->streamFilename, commandArgs->inputFilenames, commandArgs->inputFilenameCount, heap ) ? 0 : 1;
}

int Main_HandleTrim( const CommandArgs* commandArgs, IAllocator* heap )
{
	return VideoStream_Trim( commandArgs->streamFilename, commandArgs->inputFilenames[0], commandArgs->firstSequenceToRemoveCount, commandArgs->lastSequenceToRemoveCount, heap ) ? 0 : 1;
}

int Main_HandleSequence( const CommandArgs* commandArgs, IAllocator* heap )
{
	u64 startTick = Tick_GetCount();

	if ( !VideoStream_Encode( commandArgs->streamFilename, commandArgs->templateFilename, commandArgs->frameOffset, commandArgs->frameCount, commandArgs->frameRate, commandArgs->compressionQuality, commandArgs->extend, heap ) )
		return 1;

	const u64 endTick = Tick_GetCount();

	V6_MSG( "Duration: %5.3fs\n", Tick_ConvertToSeconds( endTick - startTick ) );

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
	V6_MSG( "Encoder %d.%d.%d\n\n", V6_VERSION_MAJOR, V6_VERSION_MINOR, V6_VERSION_REV );

	v6::CHeap heap;

	v6::CommandArgs commandArgs;
	v6::CommandArgs_Init( &commandArgs );
	if ( !v6::CommandArgs_Read( &commandArgs, argc, argv ) )
	{
		v6::CommandArgs_PrintUsage();
		return 1;
	}

	CommandArgs_Log( &commandArgs );

	int exitCode;
	if ( commandArgs.key )
		exitCode = Main_HandleKey( &commandArgs, &heap );
	else if ( commandArgs.inputFilenameCount > 0 )
		exitCode = Main_HandleMerge( &commandArgs, &heap );
	else if ( commandArgs.firstSequenceToRemoveCount > 0 || commandArgs.lastSequenceToRemoveCount > 0 )
		exitCode = Main_HandleTrim( &commandArgs, &heap );
	else
		exitCode = Main_HandleSequence( &commandArgs, &heap );

	v6::LogFile_Release();

	return exitCode;
}
