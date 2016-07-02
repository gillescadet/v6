#define HLSL

#include "trace_shared.h"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)

Buffer< uint > blockGroups								: REGISTER_SRV( HLSL_BLOCK_GROUP_SLOT );
StructuredBuffer< BlockRange > blockRanges				: REGISTER_SRV( HLSL_BLOCK_RANGE_SLOT );
Buffer< uint > blockPositions							: REGISTER_SRV( HLSL_BLOCK_POS_SLOT );

RWBuffer< uint > traceCells								: REGISTER_UAV( HLSL_TRACE_CELLS_SLOT );
RWBuffer< uint > traceIndirectArgs						: REGISTER_UAV( HLSL_TRACE_INDIRECT_ARGS_SLOT );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockCullStats > blockCullStats		: REGISTER_UAV( HLSL_CULL_STATS_SLOT );
#endif // #if BLOCK_GET_STATS == 1

groupshared uint gs_traceCells[HLSL_BLOCK_THREAD_GROUP_SIZE * 2];
groupshared uint gs_traceCount;
groupshared uint gs_traceBlockOffset;

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	// 90 us

	if ( GTid.x == 0 )
		gs_traceCount = 0;

	uint traceBlockOffset = trace_blockOffset( GRID_CELL_BUCKET-1 ) + trace_blockCount( GRID_CELL_BUCKET-1 );
	if ( DTid.x == 0 )
		trace_blockOffset( GRID_CELL_BUCKET ) = traceBlockOffset;

	const uint blockGroupID = c_cullBlockGroupOffset + Gid.x;
	const uint rangeID = blockGroups[blockGroupID];

	const BlockRange range = blockRanges[c_cullBlockRangeOffset + rangeID];
	const uint blockRank = DTid.x - range.firstThreadID;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_traceCount == 0

	if ( blockRank < range.blockCount && c_cullBlockGroupCount > 0 )
	{
		// 100 us (190 us)

#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockCullStats[0].blockProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		const uint blockPosID = range.blockPosOffset + blockRank;
		const uint packedBlockPos = blockPositions[blockPosID];

		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint gridMacroMask = (1 << c_cullGridMacroShift)-1;
		const uint blockPosX = range.macroGridOffset.x + ((blockPos >> (c_cullGridMacroShift*0)) & gridMacroMask);
		const uint blockPosY = range.macroGridOffset.y + ((blockPos >> (c_cullGridMacroShift*1)) & gridMacroMask);
		const uint blockPosZ = range.macroGridOffset.z + ((blockPos >> (c_cullGridMacroShift*2)) & gridMacroMask);

		uint insidePlaneCount = 0;
		{
			const uint3 cellMinCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
			const float gridScale = c_cullCentersAndGridScales[mip].w;
			const float cellSize = gridScale * 2.0f * c_cullInvGridWidth;
			const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_cullCentersAndGridScales[mip].xyz;
			const float deltaCenterWS = cellSize * 2;
			const float4 centerWS = float4( posMinWS + deltaCenterWS, 1.0f );
			for ( uint plane = 0; plane < 4; ++plane )
				insidePlaneCount += dot( centerWS, c_cullFrustumPlanes[plane] ) > 0.0f;
		}
		const bool inside = insidePlaneCount == 4;

		if ( inside )
		{
			// 50 us (240 us)

			uint traceBlockRank = 0;
			InterlockedAdd( gs_traceCount, 1, traceBlockRank );

			// 10 us (250 us)

#if 1
			const uint tracePackedBlockPos = (mip << 28) | (blockPosZ << (c_cullGridMacroShift*2)) | (blockPosY << c_cullGridMacroShift) | blockPosX;
			const uint blockDataID = range.blockDataOffset + blockRank * GRID_CELL_COUNT;
			gs_traceCells[traceBlockRank * 2 + 0] = tracePackedBlockPos;
#if BLOCK_GET_STATS == 1
			uint historyColor;
			if ( range.frameDistance == 0 )
				historyColor = 0;
			else if ( range.frameDistance < 5 )
				historyColor = 1;
			else if ( range.frameDistance < 10 )
				historyColor = 2;
			else
				historyColor = 3;
			gs_traceCells[traceBlockRank * 2 + 1] = (historyColor << 30) | blockDataID;
#else
			gs_traceCells[traceBlockRank * 2 + 1] = blockDataID;
#endif

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
			InterlockedAdd( blockCullStats[0].cellOutputCounts[mip], GRID_CELL_COUNT );
#endif // #if BLOCK_GET_STATS == 1

#endif
		}
	}

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_traceCount is up to date
	
	if ( GTid.x == 0 && gs_traceCount > 0 )
	{
		uint traceBlockCount = 0;
		InterlockedAdd( trace_blockCount( GRID_CELL_BUCKET ), gs_traceCount, traceBlockCount );
		gs_traceBlockOffset = traceBlockOffset + traceBlockCount;
	}

	GroupMemoryBarrierWithGroupSync(); // ensure that gs_traceBlockOffset is up to date

	if ( GTid.x < gs_traceCount )
	{
		const uint traceBlockID = gs_traceBlockOffset + GTid.x;
		traceCells[traceBlockID * 2 + 0] = gs_traceCells[GTid.x * 2 + 0];
		traceCells[traceBlockID * 2 + 1] = gs_traceCells[GTid.x * 2 + 1];
	}
}
