#define HLSL
#include "common_shared.h"

StructuredBuffer< Sample > collectedSamples	: register( HLSL_COLLECTED_SAMPLE_UAV );
RWStructuredBuffer< Sample > sortedSamples	: register( HLSL_SORTED_SAMPLE_UAV );
RWBuffer< uint > sampleIndirectArgs			: register( HLSL_SAMPLE_INDIRECT_ARGS_UAV );

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint collectedSampleID = DTid.x;
	
	[branch]
	if ( (collectedSamples[collectedSampleID].row1 & 0xF) != c_currentMip )
		return;

	uint sortedSampleID;
	InterlockedAdd( sample_cellPerLevelCount( c_currentMip ), 1, sortedSampleID );

	sortedSamples[sortedSampleID] = collectedSamples[collectedSampleID];	
}
