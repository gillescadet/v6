#define HLSL
#include "common_shared.h"
#include "sample_pack.hlsli"

StructuredBuffer< Sample > collectedSamples	: register( HLSL_COLLECTED_SAMPLE_SRV );
Buffer< uint > collectedSampleIndirectArgs	: register( HLSL_COLLECTED_SAMPLE_INDIRECT_ARGS_SRV );
RWStructuredBuffer< Sample > sortedSamples	: register( HLSL_SORTED_SAMPLE_UAV );
RWBuffer< uint > sortedSampleIndirectArgs	: register( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_UAV );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint collectedSampleID = DTid.x;

#if 1
	[branch]
	if ( collectedSampleID >= sample_count || Sample_UnpackMip( collectedSamples[collectedSampleID] ) != c_currentMip )
		return;
#endif

	uint sortedSampleID;
	InterlockedAdd( sample_cellPerLevelCount( c_currentMip ), 1, sortedSampleID );

	sortedSamples[sortedSampleID] = collectedSamples[collectedSampleID];	
}
