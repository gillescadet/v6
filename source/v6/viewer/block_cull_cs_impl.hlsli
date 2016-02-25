#define HLSL

#include "common_shared.h"
#include "block_encoding.hlsli"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)

Buffer< uint > blockPositions							: register( HLSL_BLOCK_POS_SRV );
Buffer< uint > blockData								: register( HLSL_BLOCK_DATA_SRV );
Buffer< uint > blockIndirectArgs						: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );

RWBuffer< uint > traceCells								: register( HLSL_TRACE_CELLS_UAV );
RWBuffer< uint > traceIndirectArgs						: register( HLSL_TRACE_INDIRECT_ARGS_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockCullStats > blockCullStats		: register( HLSL_CULL_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
#if HLSL_ENCODE_DATA == 0
	const uint blockOffset = trace_blockOffset( GRID_CELL_BUCKET-1 ) + trace_blockCount( GRID_CELL_BUCKET-1 );
	if ( DTid.x == 0 )
		trace_blockOffset( GRID_CELL_BUCKET ) = blockOffset;
#endif // #if HLSL_ENCODE_DATA == 0

	const uint blockID = DTid.x;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockCullStats[0].blockInputCount, 1 );
#endif // #if BLOCK_GET_STATS == 1	

	if ( blockID < block_count( GRID_CELL_BUCKET ) )
	{
		const uint posOffset = block_posOffset( GRID_CELL_BUCKET );	
		const uint blockPosID = posOffset + blockID;	

		const uint packedBlockPos = blockPositions[blockPosID];
#if 1
		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint xMin = ((blockPos >> 0)							& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
		const uint yMin = ((blockPos >> HLSL_GRID_MACRO_SHIFT)		& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
		const uint zMin = ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
		const int3 cellMinCoords = int3( xMin, yMin, zMin );	
		const float gridScale = c_cullGridScales[mip].x;
		const float cellSize = gridScale * 2.0f * HLSL_GRID_INV_WIDTH;
		const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_cullCenter;
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
#endif
		{
#if HLSL_ENCODE_DATA == 1
			const uint dataSizePerBucket[] = { 4, 4, 4 , 5 , 7  };
			const uint dataSize = dataSizePerBucket[GRID_CELL_BUCKET];
			const uint firstDataOffset = block_dataOffset( GRID_CELL_BUCKET );	
			const uint blockDataID = firstDataOffset + blockID * dataSize;

			const uint endPointColors = blockData[blockDataID+0];
			uint presenceBits = blockData[blockDataID+1];
			
			uint cellPosPacked[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			uint cellPosOffset = 0;
			uint cellCount = 0;
			
			for (;;)
			{
				if ( presenceBits == 0 && cellPosOffset == 0 )
				{
					presenceBits = blockData[blockDataID+2];
					cellPosOffset = 32;
				}

				int cellPos = firstbitlow( presenceBits );
				if ( cellPos == -1 )
					break;

				presenceBits -= (1 << cellPos);
				cellPos += cellPosOffset;

				const uint cellPosBucket = cellCount >> 2;
				const uint cellPosShift = (cellCount & 3) << 3;
				cellPosPacked[cellPosBucket] |= cellPos << cellPosShift;
				++cellCount;
			}

			uint cellBaseID = 0;  
			InterlockedAdd( trace_cellCount, cellCount, cellBaseID );
			cellBaseID *= 2;
						
			uint paletteColors[4];
			Block_DecodeColors( endPointColors, paletteColors );

			const uint cellColorBucketCount = (cellCount + 15) >> 4;
			uint cellRank = 0;
			for ( uint cellColorBucket = 0; cellColorBucket < cellColorBucketCount; ++cellColorBucket )
			{
				uint cellColorIndices = blockData[blockDataID+3+cellColorBucket];
				for ( uint cellColorKey = 0; cellColorKey < 16 && cellRank < cellCount; ++cellColorKey, ++cellRank, cellColorIndices >>= 2 )
				{					
					const uint cellPosBucket = cellRank >> 2;
					const uint cellPos = cellPosPacked[cellPosBucket] & 0xFF;
					cellPosPacked[cellPosBucket] >>= 8;

					const uint colorID = cellColorIndices & 3;				
					const uint cellRGB_none = paletteColors[colorID];

					// 340us

					//if ( packedBlockPos == 0x7777777 )
					{
						// optimization: find a way to write the block pos only once
						traceCells[cellBaseID + cellRank*2 + 0] = packedBlockPos;
						traceCells[cellBaseID + cellRank*2 + 1] = cellRGB_none | cellPos;
					}
				}
			}

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
			InterlockedAdd( blockCullStats[0].cellOutputCount, cellCount );
#endif // #if BLOCK_GET_STATS == 1

#else // #if HLSL_ENCODE_DATA == 1
			uint blockRank = 0;  
			InterlockedAdd( trace_blockCount( GRID_CELL_BUCKET ), 1, blockRank );
				
			traceCells[blockOffset + blockRank] = blockID;

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockCullStats[0].blockPassedCount, 1 );
			InterlockedAdd( blockCullStats[0].cellOutputCount, GRID_CELL_COUNT );
#endif // #if BLOCK_GET_STATS == 1

#endif // #if HLSL_ENCODE_DATA == 0
		}
	}
}
