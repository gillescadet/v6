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

#if 1

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID )
{
	const uint traceBlockOffset = trace_blockOffset( GRID_CELL_BUCKET-1 ) + trace_blockCount( GRID_CELL_BUCKET-1 );
	if ( DTid.x == 0 )
		trace_blockOffset( GRID_CELL_BUCKET ) = traceBlockOffset;

	const uint blockGroupID = c_cullBlockGroupOffset + Gid.x;
	const uint rangeID = blockGroups[blockGroupID];

	const BlockRange range = blockRanges[c_cullBlockRangeOffset + rangeID];
	const uint blockRank = DTid.x - range.firstThreadID;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

	if ( blockRank < range.blockCount && c_cullBlockGroupCount > 0 )
	{
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
		const uint3 cellMinCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
		const float gridScale = c_cullCentersAndGridScales[mip].w;
		const float cellSize = gridScale * 2.0f * c_cullInvGridWidth;
		const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_cullCentersAndGridScales[mip].xyz;
		const float deltaWS = cellSize * 4;

		const float3 vertices[8] =
		{
			float3( 0.0f, 0.0f, 0.0f ),
			float3( 0.0f, 0.0f, 1.0f ),
			float3( 0.0f, 1.0f, 0.0f ),
			float3( 0.0f, 1.0f, 1.0f ),
			float3( 1.0f, 0.0f, 0.0f ),
			float3( 1.0f, 0.0f, 1.0f ),
			float3( 1.0f, 1.0f, 0.0f ),
			float3( 1.0f, 1.0f, 1.0f ),
		};

		bool inside = false;
		for ( uint vertexID = 0; vertexID < 8; ++vertexID )
		{
			const float4 posWS = float4( posMinWS + deltaWS * vertices[vertexID], 1.0f );
			uint insidePlaneCount = 0; 
			for ( uint plane = 0; plane < 4; ++plane )
				insidePlaneCount += dot( posWS, c_cullFrustumPlanes[plane] ) > 0.0f;
			if ( insidePlaneCount == 4 )
			{
				inside = true;
				break;
			}
		}

		if ( inside )
		{
			uint traceBlockRank = 0;
			InterlockedAdd( trace_blockCount( GRID_CELL_BUCKET ), 1, traceBlockRank );

			const uint tracePackedBlockPos = (mip << 28) | (blockPosZ << (c_cullGridMacroShift*2)) | (blockPosY << c_cullGridMacroShift) | blockPosX;
			const uint traceBlockID = traceBlockOffset + traceBlockRank;
			const uint blockDataID = range.blockDataOffset + blockRank * GRID_CELL_COUNT;
			traceCells[traceBlockID * 2 + 0] = tracePackedBlockPos;
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
			traceCells[traceBlockID * 2 + 1] = (historyColor << 30) | blockDataID;
#else
			traceCells[traceBlockID * 2 + 1] = blockDataID;
#endif

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
			InterlockedAdd( blockCullStats[0].cellOutputCounts[mip], GRID_CELL_COUNT );
#endif // #if BLOCK_GET_STATS == 1
		}
	}
}

#elif 0

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID )
{
	// 1840

	// A: 75us (75us)

	// dispatch calls

	// B: 65us (140us)

	const uint traceBlockOffset = trace_blockOffset( GRID_CELL_BUCKET-1 ) + trace_blockCount( GRID_CELL_BUCKET-1 );
	if ( DTid.x == 0 )
		trace_blockOffset( GRID_CELL_BUCKET ) = traceBlockOffset;

	const uint blockGroupID = c_cullBlockGroupOffset + Gid.x;
	const uint rangeID = blockGroups[blockGroupID];

	const BlockRange range = blockRanges[c_cullBlockRangeOffset + rangeID];
	const uint blockRank = DTid.x - range.firstThreadID;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

	// C: 1700us (1840us)

	if ( blockRank < range.blockCount && c_cullBlockGroupCount > 0 )
	{
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockCullStats[0].blockProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		// C1: 1460us (1600us)

		// bottleneck

		const uint blockPosID = range.blockPosOffset + blockRank;
		const uint packedBlockPos = blockPositions[DTid.x];

		// C2: 160us (0us)

		if ( packedBlockPos == 0xFFFFFFFF )
		{
#if 0
			uint traceBlockRank = 0;
			InterlockedAdd( trace_blockCount( GRID_CELL_BUCKET ), 1, traceBlockRank );

			const uint tracePackedBlockPos = (mip << 28) | (blockPosZ << (c_cullGridMacroShift*2)) | (blockPosY << c_cullGridMacroShift) | blockPosX;
			const uint traceBlockID = traceBlockOffset + traceBlockRank;
			const uint blockDataID = range.blockDataOffset + blockRank * GRID_CELL_COUNT;
			traceCells[traceBlockID * 2 + 0] = tracePackedBlockPos;
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
			traceCells[traceBlockID * 2 + 1] = (historyColor << 30) | blockDataID;
#else
			traceCells[traceBlockID * 2 + 1] = blockDataID;
#endif

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
			InterlockedAdd( blockCullStats[0].cellOutputCounts[mip], GRID_CELL_COUNT );
#endif // #if BLOCK_GET_STATS == 1
#else
			traceCells[DTid.x] = 1;
#endif
		}
	}
}

#else

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID )
{
	const uint packedBlockPos = blockPositions[DTid.x];
	if ( packedBlockPos == 0xFFFFFFFF )
		InterlockedAdd( traceCells[0], packedBlockPos );
}

#endif