#define HLSL

#include "common_shared.h"

Buffer< uint > blockPositions		: register( HLSL_BLOCK_POS_SRV );
Buffer< uint > blockIndirectArgs	: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );

RWBuffer< uint > streamGroupBits	: register( HLSL_STREAM_GROUP_BITS_UAV );
RWBuffer< uint > streamBits			: register( HLSL_STREAM_BITS_UAV );

#define EXPORT_STREAM_SET_BIT 1
#include "stream_scan_cs_impl.hlsli"

[ numthreads( HLSL_STREAM_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	SetBit( Gid.x, GTid.x, DTid.x );
}
