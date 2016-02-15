#define HLSL

#include "common_shared.h"

Buffer< uint > blockPositions							: register( HLSL_BLOCK_POS_SRV );
Buffer< uint > blockData								: register( HLSL_BLOCK_DATA_SRV );
Buffer< uint > blockIndirectArgs						: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );
RWBuffer< uint > culledBlocks							: register( HLSL_TRACE_CULLED_BLOCK_UAV );
RWBuffer< uint > traceIndirectArgs						: register( HLSL_TRACE_INDIRECT_ARGS_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockCullStats > blockCullStats		: register( HLSL_CULL_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1
#if HLSL_DEBUG_CULL == 1
RWStructuredBuffer< DebugCull > debugCulls				: register( HLSL_CULL_DEBUG_UAV );
#endif // #if HLSL_DEBUG_CULL == 1

#define debugCull debugCulls[0]

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint blockID = DTid.x;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1	

#if HLSL_DEBUG_CULL == 1
	const bool doDebugCull = blockID == 0;
#endif
	
	if ( blockID < block_count( GRID_CELL_BUCKET ) )
	{
		const uint posOffset = block_posOffset( GRID_CELL_BUCKET );	
		const uint blockPosID = posOffset + blockID;	

		const uint packedBlockPos = blockPositions[blockPosID];
		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint xMin = ((blockPos >> 0)							& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
		const uint yMin = ((blockPos >> HLSL_GRID_MACRO_SHIFT)		& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
		const uint zMin = ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
		const int3 cellMinCoords = int3( xMin, yMin, zMin );	
		const float gridScale = c_blockGridScales[mip].x;
		const float cellSize = gridScale * 2.0f * HLSL_GRID_INV_WIDTH;
		const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_blockCenter;
		const float deltaWS = cellSize * HLSL_GRID_BLOCK_WIDTH;

#if HLSL_DEBUG_CULL == 1
		if (doDebugCull)
		{
			debugCull.posOffset = posOffset;
			debugCull.blockPosID = blockPosID;
			debugCull.packedBlockPos = packedBlockPos;
			debugCull.mip = mip;
			debugCull.blockPos = blockPos;
			debugCull.xMin = xMin;
			debugCull.yMin = yMin;
			debugCull.zMin = zMin;
			debugCull.cellMinCoords = cellMinCoords;
			debugCull.gridScale = gridScale;
			debugCull.cellSize = cellSize;
			debugCull.posMinWS = posMinWS;
			debugCull.deltaWS = deltaWS;
		}
#endif

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

		const matrix worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );

		uint clippedCount = 0;
		for ( uint vertexID = 0; vertexID < 8; ++vertexID )
		{
			const float3 posWS = posMinWS + deltaWS * vertices[vertexID];
			const float4 posCS = mul( worldToProjMatrix, float4( posWS, 1.0f ) );
			clippedCount += (abs( posCS.x ) > posCS.w || abs( posCS.y ) > posCS.w || posCS.w < 0) ? 1 : 0;

#if HLSL_DEBUG_CULL == 1
			if (doDebugCull)
			{
				debugCull.vertices[vertexID].posWS = posWS;
				debugCull.vertices[vertexID].posCS = posCS;
				debugCull.vertices[vertexID].clippedCount = clippedCount;
			}
#endif // #if HLSL_DEBUG_CULL == 1
		}

#if HLSL_DEBUG_CULL == 1
		debugCull.clippedCount = clippedCount;
#endif // #if HLSL_DEBUG_CULL == 1

		if ( clippedCount < 8 )
		{
			uint culledBlockID;		                
			InterlockedAdd( trace_culledBlockCount, 1, culledBlockID );

			const uint dataSizePerBucket[] = { 4, 4, 4 , 5 , 7  };
			const uint dataSize = dataSizePerBucket[GRID_CELL_BUCKET];
			const uint firstDataOffset = block_dataOffset( GRID_CELL_BUCKET );	
			const uint blockDataID = firstDataOffset + blockID * dataSize;
		
			const uint culledBlockBaseID = culledBlockID * (1 + dataSize);	

			culledBlocks[culledBlockBaseID + 0] = packedBlockPos;
			for ( uint dataOffset = 0; dataOffset < dataSize; ++dataOffset )
				culledBlocks[culledBlockBaseID + 1 + dataOffset] = blockData[blockDataID + dataOffset];

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

#if HLSL_DEBUG_CULL == 1
			if (doDebugCull)
			{
				debugCull.dataSize = dataSize;
				debugCull.firstDataOffset = firstDataOffset;
				debugCull.blockDataID = blockDataID;
				debugCull.culledBlockBaseID = culledBlockBaseID;
				debugCull.culledBlocks[0] = packedBlockPos;
				for (uint dataOffset = 0; dataOffset < dataSize; ++dataOffset)
					debugCull.culledBlocks[1 + dataOffset] = blockData[blockDataID + dataOffset];
			}
#endif // #if HLSL_DEBUG_CULL == 1
		}
	}
}
