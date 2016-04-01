#define HLSL

#include "common_shared.h"
#include "block_encoding.hlsli"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)

Buffer< uint > blockGroups								: register( HLSL_BLOCK_GROUP_SRV );
StructuredBuffer< BlockRange > blockRanges				: register( HLSL_BLOCK_RANGE_SRV );
Buffer< uint > blockPositions							: register( HLSL_BLOCK_POS_SRV );

RWBuffer< uint > traceCells								: register( HLSL_TRACE_CELLS_UAV );
RWBuffer< uint > traceIndirectArgs						: register( HLSL_TRACE_INDIRECT_ARGS_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockCullStats > blockCullStats		: register( HLSL_CULL_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID )
{
	const uint traceBlockOffset = trace_blockOffset( GRID_CELL_BUCKET-1 ) + trace_blockCount( GRID_CELL_BUCKET-1 );
	if ( DTid.x == 0 )
		trace_blockOffset( GRID_CELL_BUCKET ) = traceBlockOffset;

	const uint blockGroupID = c_cullBlockGroupOffset + Gid.x;
	const uint rangeID = blockGroups[blockGroupID];

	const BlockRange range = blockRanges[rangeID];
	const uint blockRank = DTid.x - range.firstThreadID;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1	

	if ( blockRank < range.blockCount )
	{
		const uint blockPosID = range.blockPosOffset + blockRank;
		const uint packedBlockPos = blockPositions[blockPosID];

		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint blockPosX = range.gridOffset.x + ((blockPos >> 0)						& HLSL_GRID_MACRO_MASK);
		const uint blockPosY = range.gridOffset.y + ((blockPos >> HLSL_GRID_MACRO_SHIFT)	& HLSL_GRID_MACRO_MASK);
		const uint blockPosZ = range.gridOffset.z + ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK);
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
			traceCells[traceBlockID * 2 + 1] = blockDataID;

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
			InterlockedAdd( blockCullStats[0].cellOutputCount, GRID_CELL_COUNT );
#endif // #if BLOCK_GET_STATS == 1
		}
	}
}
