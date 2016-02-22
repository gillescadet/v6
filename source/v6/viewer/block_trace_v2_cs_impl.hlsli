#define HLSL

#include "common_shared.h"
#include "block_encoding.hlsli"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

#if HLSL_ENCODE_DATA == 0
Buffer< uint > blockPositions								: register( HLSL_BLOCK_POS_SRV );
Buffer< uint > blockData									: register( HLSL_BLOCK_DATA_SRV );
Buffer< uint > blockIndirectArgs							: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );
#endif  // #if HLSL_ENCODE_DATA == 0

Buffer< uint > traceCells									: register( HLSL_TRACE_CELLS_SRV );
Buffer< uint > traceIndirectArgs							: register( HLSL_TRACE_INDIRECT_ARGS_SRV );

RWStructuredBuffer< BlockCellItem > blockCellItems			: register( HLSL_BLOCK_CELL_ITEM_UAV );
RWBuffer< uint > firstBlockCellItemIDs						: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV );
RWBuffer< uint > blockCellItemCounters						: register( HLSL_BLOCK_CELL_ITEM_COUNT_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats		: register( HLSL_TRACE_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1

bool TraceCell( int2 pixelCoords, int x, int y, float3 boxMinRS, float3 boxMaxRS )
{
	const int2 frameSize = int2( c_blockFrameSize.xy );

	const int2 otherPixelCoords = pixelCoords + uint2( x, y );
	const float3 rayDir = mad( otherPixelCoords.y, c_blockRayDirUp, mad( otherPixelCoords.x, c_blockRayDirRight, c_blockRayDirBase ) );
	const float3 rayInvDir = rcp( rayDir );
	const float3 t0 = boxMinRS * rayInvDir;
	const float3 t1 = boxMaxRS * rayInvDir;
	const float3 tMin = min( t0, t1 );
	const float3 tMax = max( t0, t1 );
	const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
	const float tOut = min( min( tMax.x, tMax.y ), tMax.z );	
	const bool hit = (tIn <= tOut) && (otherPixelCoords.x >= 0 && otherPixelCoords.x < frameSize.x && otherPixelCoords.y >= 0 && otherPixelCoords.y < frameSize.y);
	return hit;
}

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint cellID = DTid.x;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockTraceStats[0].cellInputCount, 1 );	
#endif // #if BLOCK_GET_STATS == 1

#if HLSL_ENCODE_DATA == 1
	if ( cellID < trace_cellCount )
#else
	if ( cellID < trace_blockCount( GRID_CELL_BUCKET ) * GRID_CELL_COUNT )
#endif
	{
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockTraceStats[0].cellProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		bool valid;
		uint rgb_none;
		float3 boxMinRS;
		float3 boxMaxRS;
		uint pixelDepth21;
		int2 pixelCoords;		

		{
#if HLSL_ENCODE_DATA == 1
			const uint packedPos = traceCells[cellID*2+0];
			const uint packedColor = traceCells[cellID*2+1];
			valid = true;
#else // #if HLSL_ENCODE_DATA == 1
			const uint blockRank = cellID >> GRID_CELL_SHIFT;
			const uint cellRank = cellID & GRID_CELL_MASK;

			const uint blockRankOffset = trace_blockOffset( GRID_CELL_BUCKET );	
			const uint blockID = traceCells[blockRankOffset + blockRank];

			const uint blockPosOffset = block_posOffset( GRID_CELL_BUCKET );	
			const uint packedPos = blockPositions[blockPosOffset + blockID];

			const uint firstDataOffset = block_dataOffset( GRID_CELL_BUCKET );	
			const uint packedColor = blockData[firstDataOffset + blockID * GRID_CELL_COUNT + cellRank];

			valid = packedColor != HLSL_GRID_BLOCK_CELL_EMPTY;
#endif // #if HLSL_ENCODE_DATA == 0
			const uint mip = packedPos >> 28;
			const uint blockPos = packedPos & 0x0FFFFFFF;
			rgb_none = packedColor & ~0xFF;
			const uint cellPos = packedColor & 0x3F;
			const uint x = (((blockPos >> 0						 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
			const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
			const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);
			const uint3 cellCoords = uint3( x, y, z );	
			
			const float gridScale = c_blockGridScales[mip].x;
			const float halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
			const float3 cellPosWS = mad( cellCoords, halfCellSize * 2.0, -gridScale + halfCellSize ) + c_blockCenter;			
			
			const float3 rayOrgWS = float3( c_blockViewToObject[0].w, c_blockViewToObject[1].w, c_blockViewToObject[2].w );
			const float3 cellPosRS = cellPosWS - rayOrgWS; // optimization: do everything in camera relative space
			const float cellScale = c_blockGridScales[mip].y;
			boxMinRS = cellPosRS - cellScale;
			boxMaxRS = cellPosRS + cellScale;

			const matrix worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );
			const float4 cellPosCS = mul( worldToProjMatrix, float4( cellPosWS, 1.0f ) );
			const float2 cellScreenPos = cellPosCS.xy * rcp( cellPosCS.w );			
			
			pixelDepth21 = uint( cellPosCS.w * c_blockGridScales[HLSL_MIP_MAX_COUNT-1].z );

			const float2 pixelPos = mad( cellScreenPos, 0.5f, 0.5f ) * c_blockFrameSize;
			pixelCoords = int2( pixelPos );
		}

		if ( valid && TraceCell( pixelCoords, 0, 0, boxMinRS, boxMaxRS ) )
		{
			uint hitMask8 = 0;
			// 0.35 ms
			// if ( boxMinRS.x == 66666.66666f ) 
			{
				hitMask8 |= TraceCell( pixelCoords, -1, -1, boxMinRS, boxMaxRS ) << 0;
				hitMask8 |= TraceCell( pixelCoords,  0, -1, boxMinRS, boxMaxRS ) << 1;			
				hitMask8 |= TraceCell( pixelCoords, +1, -1, boxMinRS, boxMaxRS ) << 2;
				hitMask8 |= TraceCell( pixelCoords, -1,  0, boxMinRS, boxMaxRS ) << 3;
				hitMask8 |= TraceCell( pixelCoords, +1,  0, boxMinRS, boxMaxRS ) << 4;
				hitMask8 |= TraceCell( pixelCoords, -1, +1, boxMinRS, boxMaxRS ) << 5;
				hitMask8 |= TraceCell( pixelCoords,  0, +1, boxMinRS, boxMaxRS ) << 6;
				hitMask8 |= TraceCell( pixelCoords, +1, +1, boxMinRS, boxMaxRS ) << 7;
			}

			// 1.80 ms
			// if ( boxMinRS.x == 77777.77777f ) 
			{
				const int2 frameSize = int2( c_blockFrameSize.xy );
				const uint blockCellItemBucket = mad( pixelCoords.y >> 3, frameSize.x >> 3, pixelCoords.x >> 3 );			

				uint blockCellItemID = 0;
				InterlockedAdd( blockCellItemCounters[blockCellItemBucket], 1, blockCellItemID );
				blockCellItemID += 1;

				if ( blockCellItemID < HLSL_CELL_ITEM_PER_BUCKET_MAX_COUNT )
				{
					const uint pixelID = mad( pixelCoords.y, frameSize.x, pixelCoords.x );

					uint nextBlockCellItemID;
					InterlockedExchange( firstBlockCellItemIDs[pixelID], blockCellItemID, nextBlockCellItemID );

					BlockCellItem blockCellItem = (BlockCellItem)0;			
					blockCellItem.depth21_nextID11 = (pixelDepth21 << 11) | nextBlockCellItemID;
					blockCellItem.r8g8b8_hitMask8 = rgb_none | hitMask8;

					const uint blockCellItemPage = blockCellItemID >> 8;
					const uint blockCellItemSubID = blockCellItemID & 0xFF;
					const uint blockCellSubBucketMaxCount = (frameSize.x >> 3) * (frameSize.y >> 3);
					const uint blockCellItemOffset = mad( blockCellItemPage, blockCellSubBucketMaxCount, blockCellItemBucket ) * HLSL_CELL_ITEM_PER_SUB_BUCKET_MAX_COUNT;
					const uint blockCellItemAddress = blockCellItemOffset + blockCellItemSubID;
					blockCellItems[blockCellItemAddress] = blockCellItem;

#if BLOCK_GET_STATS	== 1
					InterlockedAdd( blockTraceStats[0].pixelSampleCount, 1 + countbits( hitMask8 ) );
					InterlockedAdd( blockTraceStats[0].cellItemCount, 1 );
#endif // #if BLOCK_GET_STATS == 1
				}

#if BLOCK_GET_STATS	== 1
				InterlockedMax( blockTraceStats[0].cellItemMaxCountPerBucket, blockCellItemID );
#endif // #if BLOCK_GET_STATS == 1
			}
		}
	}
}
