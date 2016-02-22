#if HLSL_ENCODE_DATA == 1
#define	BLOCK_GET_STATS	1
#include "block_trace_v2_cs_impl.hlsli"
#else
[ numthreads( 1, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
}
#endif
