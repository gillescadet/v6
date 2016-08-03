#define HLSL

#include "trace_shared.h"

StructuredBuffer< VisibleBlock > visibleBlocks				: REGISTER_SRV( HLSL_VISIBLE_BLOCK_SLOT );
Buffer< uint > visibleBlockContext							: REGISTER_SRV( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT );

RWBuffer< uint > blockPatchCounters							: REGISTER_UAV( HLSL_BLOCK_PATCH_COUNTERS_SLOT );
RWStructuredBuffer< BlockPatch > blockPatches				: REGISTER_UAV( HLSL_BLOCK_PATCHES_SLOT );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockProjectStats > blockProjectStats	: REGISTER_UAV( HLSL_PROJECT_STATS_SLOT );
#endif // #if BLOCK_GET_STATS == 1

struct BlockPatchHeader
{
	uint	xtile16_ytile16;
	uint	blockPosID24_none8;
	uint	packedBlockPos;
};

struct BlockPatchDetail
{
	uint	blockRank6_none6_itile2_jtile2_x4_y4_w4_h4;
};

groupshared BlockPatchHeader	gs_patchHeaders[HLSL_BLOCK_THREAD_GROUP_SIZE];
groupshared BlockPatchDetail	gs_patchDetails[HLSL_BLOCK_THREAD_GROUP_SIZE * 9];
groupshared uint				gs_visibleBlockCount;
groupshared uint				gs_patchHeaderCount;
groupshared uint				gs_patchDetailCount;

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	if ( GTid.x == 0 )
	{
		gs_visibleBlockCount = visibleBlockContext[VISIBLEBLOCKCONTEXT_COUNT_OFFSET]; 
		gs_patchHeaderCount = 0;
		gs_patchDetailCount = 0;
	}

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockProjectStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_patchCount == 0

	if ( DTid.x < gs_visibleBlockCount )
	{
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockProjectStats[0].blockProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		const VisibleBlock visibleBlock = visibleBlocks[DTid.x];
		const uint blockPosID = visibleBlock.blockPosID;
		const uint packedBlockPos = visibleBlock.packedBlockPos;

		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint gridMacroMask = (1 << c_projectGridMacroShift)-1;
		const uint blockPosX = (blockPos >> (c_projectGridMacroShift*0)) & gridMacroMask;
		const uint blockPosY = (blockPos >> (c_projectGridMacroShift*1)) & gridMacroMask;
		const uint blockPosZ = (blockPos >> (c_projectGridMacroShift*2)) & gridMacroMask;

		const float gridScale = c_projectCentersAndGridScales[mip].w;
		const float cellSize = gridScale * 2.0f * c_projectInvGridWidth;
		const uint3 cellMinCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
		const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_projectCentersAndGridScales[mip].xyz;

		const float blockSize = cellSize * 4.0f;
		float2 minBlockScreenPos = float2( HLSL_FLT_MAX, HLSL_FLT_MAX );
		float2 maxBlockScreenPos = float2( -HLSL_FLT_MAX, -HLSL_FLT_MAX );

		for ( uint vertexID = 0; vertexID < 8; ++vertexID )
		{
			float3 vertexPosWS = posMinWS;
			vertexPosWS.x += (vertexID & 1) ? blockSize : 0.0f;
			vertexPosWS.y += (vertexID & 2) ? blockSize : 0.0f;
			vertexPosWS.z += (vertexID & 4) ? blockSize : 0.0f;

			float4 vertexPosCS;
			vertexPosCS.x = dot( c_projectCurWorldToProjX.xyz, vertexPosWS ) + c_projectCurWorldToProjX.w;
			vertexPosCS.y = dot( c_projectCurWorldToProjY.xyz, vertexPosWS ) + c_projectCurWorldToProjY.w;
			vertexPosCS.w = dot( c_projectCurWorldToProjW.xyz, vertexPosWS ) + c_projectCurWorldToProjW.w;

			const float2 vertexScreenPos = vertexPosCS.xy * rcp( vertexPosCS.w );
			minBlockScreenPos = min( minBlockScreenPos, vertexScreenPos );
			maxBlockScreenPos = max( maxBlockScreenPos, vertexScreenPos );
		}

		uint blockRank;
		InterlockedAdd( gs_patchHeaderCount, 1, blockRank );

		const uint blockPosID24_none8 = blockPosID << 8;
		const uint blockRank6_none26 = blockRank << 26;

		const uint2 minBlockPixelCoords = uint2( saturate( mad( minBlockScreenPos, 0.5f, 0.5f ) ) * c_projectFrameSize );
		const uint2 maxBlockPixelCoords = uint2( saturate( mad( maxBlockScreenPos, 0.5f, 0.5f ) ) * c_projectFrameSize );

#if BLOCK_GET_STATS == 1
		{
			const uint w = 1 + maxBlockPixelCoords.x - minBlockPixelCoords.x;
			const uint h = 1 + maxBlockPixelCoords.y - minBlockPixelCoords.y;
			InterlockedAdd( blockProjectStats[0].blockPatchHeaderPixelCount, w * h );
		}
#endif // #if BLOCK_GET_STATS == 1

		const uint2 minBlockTileCoords = minBlockPixelCoords >> 3;
		const uint2 maxBlockTileCoords = maxBlockPixelCoords >> 3;

		{
			BlockPatchHeader patchHeader;
			patchHeader.xtile16_ytile16 = (minBlockTileCoords.x << 16) | minBlockTileCoords.y;
			patchHeader.blockPosID24_none8 = blockPosID24_none8;
			patchHeader.packedBlockPos = packedBlockPos;

			gs_patchHeaders[blockRank] = patchHeader;
		}

		const uint2 blockTileRect = 1 + maxBlockTileCoords - minBlockTileCoords;

		const uint patchDetailCount = blockTileRect.x * blockTileRect.y;
				
		uint patchDetailID;
		InterlockedAdd( gs_patchDetailCount, patchDetailCount, patchDetailID );

		const uint2 minBlockGroupCoords = minBlockPixelCoords & 7;
		const uint2 maxBlockGroupCoords = maxBlockPixelCoords & 7;

		for ( uint tileJ = 0; tileJ < blockTileRect.y; ++tileJ )
		{
			const uint y = tileJ == 0 ? minBlockGroupCoords.y : 0;
			const uint yMax = tileJ == (blockTileRect.y-1) ? maxBlockGroupCoords.y : 7;
			const uint h = 1 + yMax - y;

			for ( uint tileI = 0; tileI < blockTileRect.x; ++tileI, ++patchDetailID )
			{
				const uint x = tileI == 0 ? minBlockGroupCoords.x : 0;
				const uint xMax = tileI == (blockTileRect.x-1) ? maxBlockGroupCoords.x : 7;
				const uint w = 1 + xMax - x;

				BlockPatchDetail patchDetail;
				patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 = blockRank6_none26 | (tileI << 18) | (tileJ << 16) | (x << 12) | (y << 8) | (w << 4) | h;

				gs_patchDetails[patchDetailID] = patchDetail;
			}
		}

#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockProjectStats[0].blockPatchHeaderCount, 1 );
		InterlockedAdd( blockProjectStats[0].blockPatchDetailCount, patchDetailCount );
#endif // #if BLOCK_GET_STATS == 1
	}

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_patchCount is up to date

	const uint pageSize = (c_projectFrameTileSize.x * c_projectFrameTileSize.y) * 64;

	for ( uint patchDetailID = GTid.x; patchDetailID < gs_patchDetailCount; patchDetailID += HLSL_BLOCK_THREAD_GROUP_SIZE )
	{
		const BlockPatchDetail patchDetail = gs_patchDetails[patchDetailID];
		const uint blockRank = patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 26;

		const BlockPatchHeader patchHeader = gs_patchHeaders[blockRank];
		const uint tileX = (patchHeader.xtile16_ytile16 >> 16) + ((patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 18) & 3);
		const uint tileY = (patchHeader.xtile16_ytile16 & 0xFFFF) + ((patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 16) & 3);
		const uint tileOffset = mad( tileY, c_projectFrameTileSize.x, tileX );

		uint patchRank = 0;
		InterlockedAdd( blockPatchCounters[tileOffset], 1, patchRank );

		if ( patchRank < HLSL_BLOCK_PATCH_MAX_COUNT_PER_TILE )
		{
			BlockPatch blockPatch;
			blockPatch.blockPosID24_x4_y4 = patchHeader.blockPosID24_none8 | ((patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 8) & 0xFF);
			blockPatch.packedBlockPos = patchHeader.packedBlockPos;
			blockPatch.none24_w4_h4 = patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 & 0xFF;

			const uint page = patchRank >> 6;
			const uint group = patchRank & 0x3F;
			const uint pageOffset =  mad( tileOffset, 64, group );
			const uint patchID = mad( page, pageSize, pageOffset );
			blockPatches[patchID] = blockPatch;

#if BLOCK_GET_STATS == 1
			const uint w = (patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 4) & 0xF;
			const uint h = (patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 0) & 0xF;
			InterlockedAdd( blockProjectStats[0].blockPatchDetailPixelCount, w * h );
			InterlockedMax( blockProjectStats[0].blockPatchMaxPage, page );
#endif // #if BLOCK_GET_STATS == 1
		}
	}
}
