/*V6*/

#include <v6/core/common.h>
#include <v6/core/decoder.h>
#include <v6/core/encoder.h>
#include <v6/core/memory.h>

#define VALIDATE (ENCODER_STRICT_BUCKET || ENCODER_STRICT_CELL)

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::CHeap heap;

#if 1
	const char* templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
	const char* sequenceFilename = "D:/media/obj/crytek-sponza/sponza.v6s";
#endif

#if 0
	const char* templateFilename = "D:/media/obj/default/default_%06d.v6f";
	const char* sequenceFilename = "D:/media/obj/default/default.v6s";
#endif

	if ( !v6::Sequence_Encode( templateFilename, 2, sequenceFilename, 75.0f / 75.0f, &heap ) )
		return 1;

#if VALIDATE
	V6_MSG( "Validating...\n" );

	v6::Sequence_s sequence = {};
	{
		v6::Stack stack( &heap, v6::MulMB( 50 ) );
		if ( !v6::Sequence_Load( sequenceFilename, &sequence, &heap, &stack ) )
			return 1;
	}

	const bool validated = v6::Sequence_Validate( templateFilename, sequenceFilename, &sequence, &heap );

	v6::Sequence_Release( &sequence, &heap );

	if ( !validated )
	{
		V6_ERROR( "Invalid sequence!\n" );
		return 1;
	}
	V6_MSG( "Well formed sequence!\n" );
#endif // #if VALIDATE

	return 0;
}