#define HLSL

#include "common_shared.h"

Buffer< uint > streamGroupBits							: register( HLSL_STREAM_GROUP_BITS_SRV );
Buffer< uint > streamCounts								: register( HLSL_STREAM_COUNTS_SRV );

RWBuffer< uint > streamAddresses						: register( HLSL_STREAM_ADDRESSES_UAV );

#define V6_ASSERT

#define EXPORT_STREAM_SUMMARIZE 1
#include "stream_scan_cs_impl.hlsli"

[ numthreads( HLSL_STREAM_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	Summarize( Gid.x, GTid.x, DTid.x );
}
