/*V6*/

#include <v6/core/common.h>
#include <v6/core/encoder.h>
#include <v6/core/memory.h>

int main()
{
	V6_MSG( "Encoder 0.0\n" );

	v6::core::CHeap heap;

	const char* templateFilename = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";
	const char* streamFilename = "D:/media/obj/crytek-sponza/sponza.v6s";

	return v6::core::Encoder_EncodeFrames( templateFilename, 75, streamFilename, &heap ) ? 0 : 1;
}