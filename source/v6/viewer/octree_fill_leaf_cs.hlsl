#include "octree_fill_leaf.hlsli"
#include "sample_pack.hlsli"

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint sampleID = DTid.x;
	
	uint3 coords;
	uint mip;
	uint3 color;
	Sample_Unpack( samples[sampleID], coords, mip, color );

	const uint leafID = sampleNodeOffsets[sampleID];
	InterlockedAdd( octreeLeaves[leafID].r, color.r );
	InterlockedAdd( octreeLeaves[leafID].g, color.g );
	InterlockedAdd( octreeLeaves[leafID].b, color.b );
	InterlockedAdd( octreeLeaves[leafID].z_count, 1 );	
}
