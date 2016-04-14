/*V6*/

#include <v6/core/common.h>
#include <v6/core/decoder.h>
#include <v6/core/encoder.h>
#include <v6/core/memory.h>

#define VALIDATE (ENCODER_STRICT_BUCKET || ENCODER_STRICT_CELL)

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::core::CHeap heap;

#if 1
	const char* templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
	const char* sequenceFilename = "D:/media/obj/crytek-sponza/sponza.v6s";
#endif

#if 0
	const char* templateFilename = "D:/media/obj/default/default_%06d.v6f";
	const char* sequenceFilename = "D:/media/obj/default/default.v6s";
#endif

	if ( !v6::core::Sequence_Encode( templateFilename, 4, sequenceFilename, &heap ) )
		return 1;

#if VALIDATE
	V6_MSG( "Validating...\n" );

	v6::core::Sequence_s sequence = {};
	if ( !v6::core::Sequence_Load( sequenceFilename, &sequence, &heap ) )
		return 1;

	const bool validated = v6::core::Sequence_Validate( templateFilename, sequenceFilename, &sequence, &heap );

	v6::core::Sequence_Release( &sequence, &heap );

	if ( !validated )
	{
		V6_ERROR( "Invalid sequence!\n" );
		return 1;
	}
	V6_MSG( "Well formed sequence!\n" );
#endif // #if VALIDATE

	return 0;
}