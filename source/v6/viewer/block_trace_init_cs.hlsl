#define HLSL

#include "common_shared.h"

RWBuffer< uint > traceIndirectArgs						: register( HLSL_TRACE_INDIRECT_ARGS_UAV );

[numthreads( 1, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint groupCount = GROUP_COUNT( trace_culledBlockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	trace_culledBlockGroupCountX = groupCount;
	trace_culledBlockGroupCountY = 1;
	trace_culledBlockGroupCountZ = 1;
}
