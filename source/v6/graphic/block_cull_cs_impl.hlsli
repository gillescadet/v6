#define HLSL

#include "trace_shared.h"
#include "onion_impl.hlsli"

Buffer< uint > blockGroups										: REGISTER_SRV( HLSL_BLOCK_GROUP_SLOT );
StructuredBuffer< BlockRange > blockRanges						: REGISTER_SRV( HLSL_BLOCK_RANGE_SLOT );
Buffer< uint > blockPositions									: REGISTER_SRV( HLSL_BLOCK_POS_SLOT );

#if CULL_ONION == 1
RWStructuredBuffer< VisibleBlockOnion > visibleBlocks			: REGISTER_UAV( HLSL_VISIBLE_BLOCK_SLOT );
#else
RWStructuredBuffer< VisibleBlockMip > visibleBlocks				: REGISTER_UAV( HLSL_VISIBLE_BLOCK_SLOT );
#endif
RWBuffer< uint > visibleBlockContext							: REGISTER_UAV( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockCullStats > blockCullStats				: REGISTER_UAV( HLSL_CULL_STATS_SLOT );
#endif // #if BLOCK_GET_STATS == 1

#if CULL_ONION == 1
groupshared VisibleBlockOnion gs_visibleBlocks[HLSL_BLOCK_THREAD_GROUP_SIZE];
#else
groupshared VisibleBlockMip gs_visibleBlocks[HLSL_BLOCK_THREAD_GROUP_SIZE];
#endif
groupshared uint gs_visibleBlockCount;
groupshared uint gs_visibleBlockOffset;

bool Assert( bool condition, uint id )
{
#if BLOCK_GET_STATS == 1
	if ( !condition )
	{
		uint prev;
		InterlockedOr( blockCullStats[0].assertFailedBits, 1 << id, prev );
		return prev == 0;
	}
	else
#endif // #if BLOCK_DEBUG == 1
	{
		return false;
	}
}

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	if ( GTid.x == 0 )
		gs_visibleBlockCount = 0;

	const uint blockGroupID = Gid.x;
	const uint rangeID = blockGroups[blockGroupID];

	const BlockRange range = blockRanges[rangeID];
	const uint blockRank = DTid.x - range.firstThreadID;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_visibleBlockCount == 0

	if ( blockRank < range.blockCount )
	{
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockCullStats[0].blockProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		const uint blockPosID = range.blockPosOffset + blockRank;
		const bool isNewBlock = range.isNewBlock & c_cullFrameChanged;

#if CULL_ONION == 1
		const uint sign1_axis2_z11_y9_x9 = blockPositions[blockPosID];
		const uint sign = (sign1_axis2_z11_y9_x9 >> 31) & 1;
		const uint axis = (sign1_axis2_z11_y9_x9 >> 29) & 3;
		uint3 blockCoords;
		blockCoords.x = (sign1_axis2_z11_y9_x9 >>  0) & 0x1FF;
		blockCoords.y = (sign1_axis2_z11_y9_x9 >>  9) & 0x1FF;
		blockCoords.z = (sign1_axis2_z11_y9_x9 >> 18) & 0x7FF;

		float3 blockPosMinRS, blockPosMaxRS;
		Onion_BlockCoordsToPos( blockPosMinRS, blockPosMaxRS, sign, axis, blockCoords, c_cullGridMinScale, c_cullInvMacroPeriodWidth, c_cullInvMacroGridWidth );
		
		const float3 centerWS = mad( blockPosMinRS + blockPosMaxRS, 0.5f, c_cullGridCenter.xyz );
#else // CULL_ONION == 0
		const uint mip4_none1_blockPos27 = blockPositions[blockPosID];

		const uint mip = mip4_none1_blockPos27 >> 28;
		
		const uint blockPos = mip4_none1_blockPos27 & 0x07FFFFFF;
		const uint blockPosX = range.macroGridOffset.x + ((blockPos >> (HLSL_MIP_MACRO_XYZ_BIT_COUNT*0)) & HLSL_MIP_MACRO_XYZ_BIT_MASK);
		const uint blockPosY = range.macroGridOffset.y + ((blockPos >> (HLSL_MIP_MACRO_XYZ_BIT_COUNT*1)) & HLSL_MIP_MACRO_XYZ_BIT_MASK);
		const uint blockPosZ = range.macroGridOffset.z + ((blockPos >> (HLSL_MIP_MACRO_XYZ_BIT_COUNT*2)) & HLSL_MIP_MACRO_XYZ_BIT_MASK);

		const uint3 cellMinCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
		const float gridScale = c_cullCentersAndGridScales[mip].w;
		const float cellSize = gridScale * 2.0f * c_cullInvGridWidth;
		const float3 posMinRS = mad( cellMinCoords, cellSize, -gridScale );
		const float3 posMinWS = posMinRS + c_cullCentersAndGridScales[mip].xyz;
		const float3 centerWS = posMinWS + cellSize * 2;
#endif // CULL_ONION == 0

		uint insidePlaneCount = 0;
		for ( uint planeID = 0; planeID < 4; ++planeID )
		{
			const float4 plane = c_cullFrustumPlanes[planeID];
			insidePlaneCount += dot( centerWS, plane.xyz ) > plane.w; // w is pre-negated
		}

		if ( insidePlaneCount == 4 )
		{
			uint visibleBlockRank = 0;
			InterlockedAdd( gs_visibleBlockCount, 1, visibleBlockRank );

#if CULL_ONION == 1
			VisibleBlockOnion visibleBlock;
			visibleBlock.sign1_axis2_z11_y9_x9 = sign1_axis2_z11_y9_x9;
#else // CULL_ONION == 1
			VisibleBlockMip visibleBlock;
			visibleBlock.mip4_none1_blockPos27 = (mip << 28) | (blockPosZ << (HLSL_MIP_MACRO_XYZ_BIT_COUNT*2)) | (blockPosY << HLSL_MIP_MACRO_XYZ_BIT_COUNT) | blockPosX;
#endif // CULL_ONION == 0
			visibleBlock.newBlock1_blockPosID = (isNewBlock << 31) | blockPosID;

			gs_visibleBlocks[visibleBlockRank] = visibleBlock;

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1
		}
	}

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_visibleBlockCount is up to date
	
	if ( GTid.x == 0 && gs_visibleBlockCount > 0 )
		InterlockedAdd( visibleBlockContext[VISIBLEBLOCKCONTEXT_COUNT_OFFSET], gs_visibleBlockCount, gs_visibleBlockOffset );

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_visibleBlockOffset is up to date

	if ( GTid.x < gs_visibleBlockCount )
	{
		const uint visibleBlockID = gs_visibleBlockOffset + GTid.x;
		visibleBlocks[visibleBlockID] = gs_visibleBlocks[GTid.x];
	}
}
