#include "grid_clear.h"

[numthreads( HLSL_GRID_CLEAR_GROUP_SIZE, 1, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint gridBlockPos = DTid.x;
	gridBlockColors[gridBlockPos] = (GridBlockColor)0;
}