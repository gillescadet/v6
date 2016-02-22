#define HLSL

#include "common_shared.h"

RWBuffer< uint > traceIndirectArgs						: register( HLSL_TRACE_INDIRECT_ARGS_UAV );

[numthreads( 1, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint cellGroupCount = GROUP_COUNT( trace_cellCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	trace_cellGroupCountX = cellGroupCount;
	trace_cellGroupCountY = 1;
	trace_cellGroupCountZ = 1;
}
