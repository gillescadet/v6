#define HLSL

#include "trace_shared.h"

Buffer< uint > blockGroups										: REGISTER_SRV( HLSL_BLOCK_GROUP_SLOT );
StructuredBuffer< BlockRange > blockRanges						: REGISTER_SRV( HLSL_BLOCK_RANGE_SLOT );
Buffer< uint > blockPositions									: REGISTER_SRV( HLSL_BLOCK_POS_SLOT );

RWStructuredBuffer< VisibleBlock > visibleBlocks				: REGISTER_UAV( HLSL_VISIBLE_BLOCK_SLOT );
RWBuffer< uint > visibleBlockContext							: REGISTER_UAV( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockCullStats > blockCullStats				: REGISTER_UAV( HLSL_CULL_STATS_SLOT );
#endif // #if BLOCK_GET_STATS == 1

groupshared VisibleBlock gs_visibleBlocks[HLSL_BLOCK_THREAD_GROUP_SIZE];
groupshared uint gs_visibleBlockCount;
groupshared uint gs_visibleBlockOffset;
groupshared uint gs_minDistanceToOrigin;

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	if ( GTid.x == 0 )
	{
		gs_visibleBlockCount = 0;
		gs_minDistanceToOrigin = 0;
	}

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
		const uint mip4_newBlock1_blockPos27 = blockPositions[blockPosID];

		const uint mip = mip4_newBlock1_blockPos27 >> 28;
		const bool isNewBlockFromData = (mip4_newBlock1_blockPos27 >> 27) & 1;
		const uint blockPos = mip4_newBlock1_blockPos27 & 0x07FFFFFF;
		const uint gridMacroMask = (1 << c_cullGridMacroShift)-1;
		const uint blockPosX = range.macroGridOffset.x + ((blockPos >> (c_cullGridMacroShift*0)) & gridMacroMask);
		const uint blockPosY = range.macroGridOffset.y + ((blockPos >> (c_cullGridMacroShift*1)) & gridMacroMask);
		const uint blockPosZ = range.macroGridOffset.z + ((blockPos >> (c_cullGridMacroShift*2)) & gridMacroMask);

		uint insidePlaneCount = 0;
		float3 posMinRS;
		{
			const uint3 cellMinCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
			const float gridScale = c_cullCentersAndGridScales[mip].w;
			const float cellSize = gridScale * 2.0f * c_cullInvGridWidth;
			posMinRS = mad( cellMinCoords, cellSize, -gridScale );
			const float3 posMinWS = posMinRS + c_cullCentersAndGridScales[mip].xyz;
			const float3 centerWS = posMinWS + cellSize * 2;
			for ( uint planeID = 0; planeID < 4; ++planeID )
			{
				const float4 plane = c_cullFrustumPlanes[planeID];
				insidePlaneCount += dot( centerWS, plane.xyz ) > plane.w; // w is pre-negated
			}
		}

		if ( insidePlaneCount == 4 )
		{
			uint visibleBlockRank = 0;
			InterlockedAdd( gs_visibleBlockCount, 1, visibleBlockRank );

			//const bool isNewBlockHack = (range.frameDistance == 0) && !c_cullIsFirstFrameOfSequence;
			const bool isNewBlockHack = 0;
			const bool isNewBlock = (isNewBlockFromData | isNewBlockHack) & c_cullFrameChanged;

			VisibleBlock visibleBlock;
			visibleBlock.blockPosID = blockPosID;
			visibleBlock.mip4_newBlock1_blockPos27 = (mip << 28) | (isNewBlock << 27) | (blockPosZ << (c_cullGridMacroShift*2)) | (blockPosY << c_cullGridMacroShift) | blockPosX;

			gs_visibleBlocks[visibleBlockRank] = visibleBlock;

			const uint distanceToOrigin = 0xFFFFFFFF - (uint)min( length( posMinRS ) * 16.0f, (float)(0xFFFFFFFF) );
			InterlockedMax( gs_minDistanceToOrigin, distanceToOrigin );

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1
		}
	}

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_visibleBlockCount is up to date
	
	if ( GTid.x == 0 && gs_visibleBlockCount > 0 )
	{
		InterlockedAdd( visibleBlockContext[VISIBLEBLOCKCONTEXT_COUNT_OFFSET], gs_visibleBlockCount, gs_visibleBlockOffset );
		InterlockedMax( visibleBlockContext[VISIBLEBLOCKCONTEXT_MIN_DISTANCE_OFFSET], gs_minDistanceToOrigin );
	}

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_visibleBlockOffset is up to date

	if ( GTid.x < gs_visibleBlockCount )
	{
		const uint visibleBlockID = gs_visibleBlockOffset + GTid.x;
		visibleBlocks[visibleBlockID] = gs_visibleBlocks[GTid.x];
	}
}
