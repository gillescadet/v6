#define HLSL

#include "trace_shared.h"

RWBuffer< uint > visibleBlockContext	: REGISTER_UAV( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT );

[numthreads( 1, 1, 1 )]
void main_block_cull_post_cs( uint3 DTid : SV_DispatchThreadID )
{
	const uint blockCount = visibleBlockContext[VISIBLEBLOCKCONTEXT_COUNT_OFFSET];
	const uint blockGroupCount = HLSL_GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	visibleBlockContext[VISIBLEBLOCKCONTEXT_GROUPCOUNTX_OFFSET] = blockGroupCount;
	visibleBlockContext[VISIBLEBLOCKCONTEXT_GROUPCOUNTY_OFFSET] = 1;
	visibleBlockContext[VISIBLEBLOCKCONTEXT_GROUPCOUNTZ_OFFSET] = 1;
}
