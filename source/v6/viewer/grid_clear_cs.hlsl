#include "grid_clear.h"

[numthreads( HLSL_GRID_CLEAR_GROUP_SIZE, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint gridBlockPos = DTid.x;
	gridBlockColors[gridBlockPos] = (GridBlockColor)0;
	gridBlockAssignedIDs[gridBlockPos] = HLSL_GRID_BLOCK_INVALID;
	if ( gridBlockPos == 0 )
	{
		gridIndirectArgs[0].indexCountPerInstance = 36;
		gridIndirectArgs[0].instanceCount = 0;
		gridIndirectArgs[0].startIndexLocation = 0;
		gridIndirectArgs[0].baseVertexLocation = 0;
		gridIndirectArgs[0].sartInstanceLocation = 0;
		gridIndirectArgs[0].blockCount = 0;
	}
}