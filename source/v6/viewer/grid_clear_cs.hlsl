#include "grid_clear.hlsli"

[numthreads( HLSL_GRID_THREAD_GROUP_SIZE, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint blockID = DTid.x;
	const uint blockCount = gridIndirectArgs_blockCount;

	if ( blockID < blockCount )
	{
		const uint gridBlockPos = gridBlockPositions[blockID];
		gridBlockIDs[gridBlockPos] = HLSL_GRID_BLOCK_INVALID;
		gridBlockColors[blockID] = (GridBlockColor)0;
	}
}