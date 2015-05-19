#include "grid_clear.h"

[numthreads( 8, 8, 8 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
	gridColors[DTid] = (float4)0;
}