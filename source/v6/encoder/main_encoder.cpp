/*V6*/

#include <v6/core/common.h>
#include <v6/core/decoder.h>
#include <v6/core/encoder.h>
#include <v6/core/memory.h>

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::core::CHeap heap;

	const char* templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
	const char* sequenceFilename = "D:/media/obj/crytek-sponza/sponza.v6s";

	if ( !v6::core::Sequence_Encode( templateFilename, 2, sequenceFilename, &heap ) )
		return 1;

#if 1
	v6::core::Sequence_s sequence = {};
	if ( !v6::core::Sequence_Load( sequenceFilename, &sequence, &heap ) )
		return 1;
	v6::core::Sequence_Release( &sequence, &heap );
#endif

	return 0;
}