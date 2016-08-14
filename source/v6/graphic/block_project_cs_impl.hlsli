#define HLSL

#include "trace_shared.h"

StructuredBuffer< VisibleBlock > visibleBlocks				: REGISTER_SRV( HLSL_VISIBLE_BLOCK_SLOT );
Buffer< uint > visibleBlockContext							: REGISTER_SRV( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT );
StructuredBuffer< uint64 > blockCellPresences				: REGISTER_SRV( HLSL_BLOCK_CELL_PRESENCE_SLOT );

RWBuffer< uint > blockPatchCounters							: REGISTER_UAV( HLSL_BLOCK_PATCH_COUNTERS_SLOT );
RWStructuredBuffer< BlockPatch > blockPatches				: REGISTER_UAV( HLSL_BLOCK_PATCHES_SLOT );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockProjectStats > blockProjectStats	: REGISTER_UAV( HLSL_PROJECT_STATS_SLOT );
#endif // #if BLOCK_GET_STATS == 1

struct BlockPatchHeader
{
	uint	blockPosID;
	uint	mip4_newBlock1_blockPos27;
	uint	xtile10_ytile10_cellmin222_cellmax222;
	uint	xdsp16_ydsp16;
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
		const uint mip4_newBlock1_blockPos27 = visibleBlock.mip4_newBlock1_blockPos27;

		uint3 cellMinRange;
		uint3 cellMaxRange;

		{
			const uint64 blockCellPresence = blockCellPresences[blockPosID];

			const uint blockCellPresenceLowAndHigh = blockCellPresence.low | blockCellPresence.high;

			{
				const uint x0 = (blockCellPresenceLowAndHigh & 0x11111111) != 0 ? 1 : 0;
				const uint x1 = (blockCellPresenceLowAndHigh & 0x22222222) != 0 ? 2 : 0;
				const uint x2 = (blockCellPresenceLowAndHigh & 0x44444444) != 0 ? 4 : 0;
				const uint x3 = (blockCellPresenceLowAndHigh & 0x88888888) != 0 ? 8 : 0;
				
				const uint xRange = x0 | x1 | x2 | x3;
				cellMinRange.x = firstbitlow( xRange );
				cellMaxRange.x = firstbithigh( xRange );
			}

			{

				const uint y0 = (blockCellPresenceLowAndHigh & 0x000F000F) != 0 ? 1 : 0;
				const uint y1 = (blockCellPresenceLowAndHigh & 0x00F000F0) != 0 ? 2 : 0;
				const uint y2 = (blockCellPresenceLowAndHigh & 0x0F000F00) != 0 ? 4 : 0;
				const uint y3 = (blockCellPresenceLowAndHigh & 0xF000F000) != 0 ? 8 : 0;
				
				const uint yRange = y0 | y1 | y2 | y3;
				cellMinRange.y = firstbitlow( yRange );
				cellMaxRange.y = firstbithigh( yRange );
			}

			{
				const uint z0 = (blockCellPresence.low  & 0x0000FFFF) != 0 ? 1 : 0;
				const uint z1 = (blockCellPresence.low  & 0xFFFF0000) != 0 ? 2 : 0;
				const uint z2 = (blockCellPresence.high & 0x0000FFFF) != 0 ? 4 : 0;
				const uint z3 = (blockCellPresence.high & 0xFFFF0000) != 0 ? 8 : 0;

				const uint zRange = z0 | z1 | z2 | z3;
				cellMinRange.z = firstbitlow( zRange );
				cellMaxRange.z = firstbithigh( zRange );
			}
		}

		const uint mip = mip4_newBlock1_blockPos27 >> 28;
		const uint blockPos = mip4_newBlock1_blockPos27 & 0x07FFFFFF;
		const uint gridMacroMask = (1 << c_projectGridMacroShift)-1;
		const uint blockPosX = (blockPos >> (c_projectGridMacroShift*0)) & gridMacroMask;
		const uint blockPosY = (blockPos >> (c_projectGridMacroShift*1)) & gridMacroMask;
		const uint blockPosZ = (blockPos >> (c_projectGridMacroShift*2)) & gridMacroMask;

		const float gridScale = c_projectCentersAndGridScales[mip].w;
		const float cellSize = gridScale * 2.0f * c_projectInvGridWidth;
		const uint3 cellCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
		const uint3 cellMinCoords = cellCoords + cellMinRange;
		const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_projectCentersAndGridScales[mip].xyz;
		const float3 posMaxWS = mad( float3( 1 + cellMaxRange - cellMinRange ), cellSize, posMinWS );

		float2 minBlockScreenPos;
		float2 maxBlockScreenPos;
		uint2 pixelDisplacementF16;

		{
			float2 prevVertexScreenPos;
			{
				float4 vertexPosCS;
				vertexPosCS.x = dot( c_projectPrevWorldToProjX.xyz, posMinWS ) + c_projectPrevWorldToProjX.w;
				vertexPosCS.y = dot( c_projectPrevWorldToProjY.xyz, posMinWS ) + c_projectPrevWorldToProjY.w;
				vertexPosCS.w = dot( c_projectPrevWorldToProjW.xyz, posMinWS ) + c_projectPrevWorldToProjW.w;
				
				prevVertexScreenPos = vertexPosCS.xy * rcp( vertexPosCS.w );
			}

			float2 curVertexScreenPos;
			{
				float4 vertexPosCS;
				vertexPosCS.x = dot( c_projectCurWorldToProjX.xyz, posMinWS ) + c_projectCurWorldToProjX.w;
				vertexPosCS.y = dot( c_projectCurWorldToProjY.xyz, posMinWS ) + c_projectCurWorldToProjY.w;
				vertexPosCS.w = dot( c_projectCurWorldToProjW.xyz, posMinWS ) + c_projectCurWorldToProjW.w;
				
				curVertexScreenPos = vertexPosCS.xy * rcp( vertexPosCS.w );
				minBlockScreenPos = curVertexScreenPos;
				maxBlockScreenPos = curVertexScreenPos;
			}

			pixelDisplacementF16 = f32tof16( (curVertexScreenPos - prevVertexScreenPos) * 0.5f * c_projectFrameSize );
		}

		for ( uint vertexID = 1; vertexID < 8; ++vertexID )
		{
			float3 vertexPosWS;
			vertexPosWS.x = (vertexID & 1) ? posMinWS.x : posMaxWS.x;
			vertexPosWS.y = (vertexID & 2) ? posMinWS.y : posMaxWS.y;
			vertexPosWS.z = (vertexID & 4) ? posMinWS.z : posMaxWS.z;

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
			patchHeader.blockPosID = blockPosID;
			patchHeader.mip4_newBlock1_blockPos27 = mip4_newBlock1_blockPos27;
			patchHeader.xtile10_ytile10_cellmin222_cellmax222 = (minBlockTileCoords.x << 22) | (minBlockTileCoords.y << 12);
			patchHeader.xtile10_ytile10_cellmin222_cellmax222 |= (cellMinRange.x << 10) | (cellMinRange.y << 8) | (cellMinRange.z << 6);
			patchHeader.xtile10_ytile10_cellmin222_cellmax222 |= (cellMaxRange.x <<  4) | (cellMaxRange.y << 2) | (cellMaxRange.z << 0);
			patchHeader.xdsp16_ydsp16 = (pixelDisplacementF16.x << 16) | pixelDisplacementF16.y;

			gs_patchHeaders[blockRank] = patchHeader;
		}

		const uint2 blockTileRect = min( 1 + maxBlockTileCoords - minBlockTileCoords, 3 );

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
		const uint tileX = ((patchHeader.xtile10_ytile10_cellmin222_cellmax222 >> 22) & 0x3FF) + ((patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 18) & 3);
		const uint tileY = ((patchHeader.xtile10_ytile10_cellmin222_cellmax222 >> 12) & 0x3FF) + ((patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 >> 16) & 3);
		const uint tileOffset = mad( tileY, c_projectFrameTileSize.x, tileX );

		uint patchRank = 0;
		InterlockedAdd( blockPatchCounters[tileOffset], 1, patchRank );

		if ( patchRank < HLSL_BLOCK_PATCH_MAX_COUNT_PER_TILE )
		{
			BlockPatch blockPatch;
			blockPatch.blockPosID = patchHeader.blockPosID;
			blockPatch.mip4_newBlock1_blockPos27 = patchHeader.mip4_newBlock1_blockPos27;
			blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 = (patchHeader.xtile10_ytile10_cellmin222_cellmax222 << 16) | (patchDetail.blockRank6_none6_itile2_jtile2_x4_y4_w4_h4 & 0xFFFF);
			blockPatch.xdsp16_ydsp16 = patchHeader.xdsp16_ydsp16;

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
