#define HLSL

#include "viewer_shared.h"

RWBuffer< uint > traceIndirectArgs						: register( HLSL_TRACE_INDIRECT_ARGS_UAV );

[numthreads( 1, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
#if HLSL_ENCODE_DATA == 1
	const uint cellGroupCount = GROUP_COUNT( trace_cellCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	trace_cellGroupCountX = cellGroupCount;
	trace_cellGroupCountY = 1;
	trace_cellGroupCountZ = 1;
#else
	for ( uint bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		const uint gridCellCount = 1 << (bucket+2);
		const uint cellCount = trace_blockCount( bucket ) * gridCellCount;
		const uint cellGroupCount = GROUP_COUNT( cellCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
		trace_cellGroupCountX( bucket ) = cellGroupCount;
		trace_cellGroupCountY( bucket ) = 1;
		trace_cellGroupCountZ( bucket ) = 1;
	}
#endif
}
