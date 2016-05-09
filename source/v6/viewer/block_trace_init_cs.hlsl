#define HLSL

#include "viewer_shared.h"

RWBuffer< uint > traceIndirectArgs						: REGISTER_UAV( HLSL_TRACE_INDIRECT_ARGS_SLOT );

[numthreads( 1, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	for ( uint bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		const uint gridCellCount = 1 << (bucket+2);
		const uint cellCount = trace_blockCount( bucket ) * gridCellCount;
		const uint cellGroupCount = GROUP_COUNT( cellCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
		trace_cellGroupCountX( bucket ) = cellGroupCount;
		trace_cellGroupCountY( bucket ) = 1;
		trace_cellGroupCountZ( bucket ) = 1;
	}
}
