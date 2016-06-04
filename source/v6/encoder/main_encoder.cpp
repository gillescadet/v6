/*V6*/

#include <v6/core/common.h>

#include <v6/codec/decoder.h>
#include <v6/codec/encoder.h>
#include <v6/core/memory.h>

#define VALIDATE (ENCODER_STRICT_BUCKET || ENCODER_STRICT_CELL)

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::CHeap heap;

#if 0
	const char* streamFilename = "D:/media/obj/crytek-sponza/sponza.v6";
	const char* templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
#endif

#if 0
	const char* streamFilename = "D:/media/obj/default/default.v6";
	const char* templateFilename = "D:/media/obj/default/default_%06d.v6f";
#endif

#if 1
	const char* streamFilename = "D:/tmp/v6/ue.v6";
	const char* templateFilename = "D:/tmp/v6/ue_%06d.v6f";
#endif

	const v6::u32 frameOffset	= 64;
	const v6::u32 frameCount	= 750 - frameOffset;
	const v6::u32 playRate		= 75;

	if ( !v6::VideoStream_Encode( streamFilename, templateFilename, frameOffset, frameCount, playRate, &heap ) )
		return 1;

#if VALIDATE
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