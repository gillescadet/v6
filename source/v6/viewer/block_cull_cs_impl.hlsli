#define HLSL

#include "viewer_shared.h"

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

	if ( blockRank < range.blockCount )
	{
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockCullStats[0].blockProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1	

		const uint blockPosID = range.blockPosOffset + blockRank;
		const uint packedBlockPos = blockPositions[blockPosID];

		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint blockPosX = range.macroGridOffset.x + ((blockPos >> 0)						& HLSL_GRID_MACRO_MASK);
		const uint blockPosY = range.macroGridOffset.y + ((blockPos >> HLSL_GRID_MACRO_SHIFT)	& HLSL_GRID_MACRO_MASK);
		const uint blockPosZ = range.macroGridOffset.z + ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK);
		const uint3 cellMinCoords = uint3( blockPosX, blockPosY, blockPosZ ) << HLSL_GRID_BLOCK_SHIFT;
		const float gridScale = c_cullGridScales[mip].x;
		const float cellSize = gridScale * 2.0f * HLSL_GRID_INV_WIDTH;
		const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_cullCenters[mip].xyz;
		const float deltaWS = cellSize * HLSL_GRID_BLOCK_WIDTH;

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

			const uint tracePackedBlockPos = (mip << 28) | (blockPosZ << HLSL_GRID_MACRO_2XSHIFT) | (blockPosY << HLSL_GRID_MACRO_SHIFT) | blockPosX;
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
			InterlockedAdd( blockCullStats[0].cellOutputCount, GRID_CELL_COUNT );
#endif // #if BLOCK_GET_STATS == 1
		}
	}
}
