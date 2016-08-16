/*V6*/

#include <v6/core/common.h>

#include <v6/codec/decoder.h>
#include <v6/codec/encoder.h>
#include <v6/core/memory.h>
#include <v6/core/time.h>

#define VALIDATE (ENCODER_STRICT_CELL)

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

END_V6_NAMESPACE

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::CHeap heap;

#if 0
	const char* streamFilename = "D:/media/obj/crytek-sponza/sponza.v6";
	const char* templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";

	const v6::u32 frameOffset	= 0;
	const v6::u32 frameCount	= 1;
	const v6::u32 playRate		= 75;
#endif

#if 0
	const char* streamFilename = "D:/media/obj/default/default.v6";
	const char* templateFilename = "D:/media/obj/default/default_%06d.v6f";

	const v6::u32 frameOffset	= 0;
	const v6::u32 frameCount	= 1;
	const v6::u32 playRate		= 75;
#endif

#if 1
	const char* streamFilename = "D:/tmp/v6/ue_1024.v6";
	const char* templateFilename = "D:/tmp/v6/ue_%06d.v6f";

	const v6::u32 frameOffset	= 0;
	const v6::u32 frameCount	= 75;
	const v6::u32 playRate		= 75;
#endif

	const v6::u64 startTick = v6::GetTickCount();

	if ( !v6::VideoStream_Encode( streamFilename, templateFilename, frameOffset, frameCount, playRate, &heap ) )
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