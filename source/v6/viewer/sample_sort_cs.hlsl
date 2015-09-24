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
	const uint sortedSampleOffset = sortedSample_sum( c_sampleCurrentMip-1 ) + sortedSample_count( c_sampleCurrentMip-1 );

	if ( DTid.x == 0 )
	{
		sortedSample_sum( c_sampleCurrentMip ) = sortedSampleOffset;
		sortedSample_groupCountY( c_sampleCurrentMip ) = 1;
		sortedSample_groupCountZ( c_sampleCurrentMip ) = 1;
	}

	const uint collectedSampleID = DTid.x;

	[branch]
	if ( collectedSampleID >= sample_count || Sample_UnpackMip( collectedSamples[collectedSampleID] ) != c_sampleCurrentMip )
		return;

	uint sortedSampleID;
	InterlockedAdd( sortedSample_count( c_sampleCurrentMip ), 1, sortedSampleID );

	uint sortedSamplePerMipCount = sortedSampleID+1;
	InterlockedMax( sortedSample_groupCountX( c_sampleCurrentMip ), GROUP_COUNT( sortedSamplePerMipCount, HLSL_SAMPLE_THREAD_GROUP_SIZE ) );

	sortedSampleID += sortedSampleOffset;

	sortedSamples[sortedSampleID] = collectedSamples[collectedSampleID];	
}
