#define HLSL

#include "common_shared.h"
#include "block_encoding.hlsli"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define BUFFER_WIDTH		HLSL_PIXEL_SUPER_SAMPLING_WIDTH

Buffer< uint > blockPosData									: register( HLSL_TRACE_CULLED_BLOCK_SRV );
Buffer< uint > traceIndirectArgs							: register( HLSL_TRACE_INDIRECT_ARGS_SRV );
RWStructuredBuffer< BlockCellItem > blockCellItems			: register( HLSL_BLOCK_CELL_ITEM_UAV );
RWBuffer< uint > firstBlockCellItemIDs						: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV );
RWBuffer< uint > blockCellItemCounters						: register( HLSL_BLOCK_CELL_ITEM_COUNT_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats		: register( HLSL_TRACE_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1

#if BUFFER_WIDTH == 1

struct CellParams
{
	uint	paletteColors[4];
	matrix	worldToProjMatrix;
	float3	rayOrgWS;
	float3	firstCellPosWS;
	float	cellSize;
	uint	blockPosDataID;
	uint	mip;
};

uint Mod3( uint n )
{
	const uint bits = 0x24924;
	return (bits >> (n * 2)) & 3;
}

uint Div3( uint n )
{
	const uint bits = 0x2A540;
	return (bits >> (n * 2)) & 3;
}

void ProcessCell( uint cellRGBA, uint cellPos, CellParams cellParams )
{
#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockTraceStats[0].cellProcessedCount, 1 );
	InterlockedAdd( blockTraceStats[0].sampleProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

	float3 cellPosRS;
	uint pixelDepth23_none9;
	int2 pixelCoords;	

	{
		const uint cellX = (cellPos >> 0						) & HLSL_GRID_BLOCK_MASK;
		const uint cellY = (cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK;
		const uint cellZ = (cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK;
		const uint3 cellCoords = int3( cellX, cellY, cellZ );

		const float3 cellPosWS = mad( cellCoords, cellParams.cellSize, cellParams.firstCellPosWS );			
		const float4 cellPosCS = mul( cellParams.worldToProjMatrix, float4( cellPosWS, 1.0f ) );
		const float2 cellScreenPos = cellPosCS.xy * rcp( cellPosCS.w );		
		cellPosRS = cellPosWS - cellParams.rayOrgWS; // optimization: do everything in camera relative space
		pixelDepth23_none9 = uint( cellPosCS.w * c_blockGridScales[HLSL_MIP_MAX_COUNT-1].z ) << 9;

		const float2 pixelPos = mad( cellScreenPos, 0.5f, 0.5f ) * c_blockFrameSize;
		pixelCoords = int2( pixelPos );
	}
	
	{
		const float3 rayEndWSR = c_blockRayDirRight;
		const float3 rayEndWSU = c_blockRayDirUp;
		const float3 rayEndRS0 = c_blockRayDirBase + (pixelCoords.y-1) * rayEndWSU + (pixelCoords.x-1) * rayEndWSR;

		const float cellScale = c_blockGridScales[cellParams.mip].y;
		const int2 frameSize = int2( c_blockFrameSize.xy );
		
		// 0.60ms / 2.20ms
		
		uint hitMask = 0;
		uint hitShift = 0;
		for ( int y = -1; y <= 1; ++y )
		{
			float3 rayDir = rayEndRS0 + (y+1) * rayEndWSU;
			for ( int x = -1; x <= 1; ++x, rayDir += rayEndWSR, ++hitShift )
			{
				const float3 rayInvDir = rcp( rayDir );
				const float3 offset = cellScale * rayInvDir;	
				const float3 t0 = mad( cellPosRS, rayInvDir, +offset );
				const float3 t1 = mad( cellPosRS, rayInvDir, -offset );
				const float3 tMin = min( t0, t1 );
				const float3 tMax = max( t0, t1 );
				const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
				const float tOut = min( min( tMax.x, tMax.y ), tMax.z );				
				const int2 otherPixelCoords = pixelCoords + uint2( x, y );
				const bool hit = (tIn <= tOut) && (otherPixelCoords.x >= 0 && otherPixelCoords.x < frameSize.x && otherPixelCoords.y >= 0 && otherPixelCoords.y < frameSize.y);
				hitMask |= hit << hitShift;
			}
		}

		// 1.00ms / 2.20ms
		// could be 0.70ms with 64bits export
		if ( hitMask & 0x10 )
		{		
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
				blockCellItem.r8g8b8a8 = cellRGBA;
				blockCellItem.depth23_hitMask9 = pixelDepth23_none9 | hitMask;
				blockCellItem.nextID = nextBlockCellItemID;
				
				const uint blockCellItemPage = blockCellItemID >> 8;
				const uint blockCellItemSubID = blockCellItemID & 0xFF;
				const uint blockCellSubBucketMaxCount = (frameSize.x >> 3) * (frameSize.y >> 3);
				const uint blockCellItemOffset = mad( blockCellItemPage, blockCellSubBucketMaxCount, blockCellItemBucket ) * HLSL_CELL_ITEM_PER_SUB_BUCKET_MAX_COUNT;
				const uint blockCellItemAddress = blockCellItemOffset + blockCellItemSubID;
				blockCellItems[blockCellItemAddress] = blockCellItem;

#if BLOCK_GET_STATS	== 1
				InterlockedAdd( blockTraceStats[0].pixelSampleCount, countbits( hitMask ) );
				InterlockedAdd( blockTraceStats[0].cellItemCount, 1 );
#endif // #if BLOCK_GET_STATS == 1
			}

#if BLOCK_GET_STATS	== 1
			InterlockedMax( blockTraceStats[0].cellItemMaxCountPerBucket, blockCellItemID );
#endif // #if BLOCK_GET_STATS == 1
		}
	}
}

void ProcessCellx16( uint cellColorIndices, CellParams cellParams, inout uint presenceBits, inout uint cellPosOffset )
{	
	for ( uint cellColorKey = 0; cellColorKey < 16; ++cellColorKey, cellColorIndices >>= 2 )
	{
		if ( presenceBits == 0 && cellPosOffset == 0 )
		{
			presenceBits = blockPosData[cellParams.blockPosDataID+3];
			cellPosOffset = 32;
		}
		int cellPos = firstbitlow( presenceBits );
		if ( cellPos != -1 )
		{
			presenceBits -= (1 << cellPos);
			cellPos += cellPosOffset;
			
			const uint colorID = cellColorIndices & 3;				
			const uint cellRGBA = cellParams.paletteColors[colorID];
				
			ProcessCell( cellRGBA, cellPos, cellParams );
		}
	}
}

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint blockID = DTid.x;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockTraceStats[0].blockInputCount, 1 );	
#endif // #if BLOCK_GET_STATS == 1

	if ( blockID < trace_culledBlockCount )
	{
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockTraceStats[0].blockProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		const uint blockSizePerBucket[] = { 5, 5, 5, 6, 8 };
		const uint blockSize = blockSizePerBucket[GRID_CELL_BUCKET];	
		const uint blockPosDataID = DTid.x * blockSize;		
		
		const uint packedBlockPos = blockPosData[blockPosDataID+0];
		const uint endPointColors = blockPosData[blockPosDataID+1];
		const uint mip = packedBlockPos >> 28;
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;		
		const uint blockX = (((blockPos >> 0					  ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT);
		const uint blockY = (((blockPos >> HLSL_GRID_MACRO_SHIFT  ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT);
		const uint blockZ = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT);
		const int3 blockCoords = int3( blockX, blockY, blockZ );
		const float gridScale = c_blockGridScales[mip].x;

		CellParams cellParams = (CellParams)0;
		cellParams.blockPosDataID = blockPosDataID;
		cellParams.mip = mip;
		cellParams.cellSize = gridScale * HLSL_GRID_INV_WIDTH * 2.0f;
		cellParams.firstCellPosWS = mad( blockCoords, cellParams.cellSize, -gridScale + cellParams.cellSize * 0.5f ) + c_blockCenter;
		cellParams.rayOrgWS = float3( c_blockViewToObject[0].w, c_blockViewToObject[1].w, c_blockViewToObject[2].w );
		cellParams.worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );
		Block_DecodeColors( endPointColors, cellParams.paletteColors );
		
		uint presenceBits = blockPosData[blockPosDataID+2];
		uint cellPosOffset = 0;
		if ( GRID_CELL_COUNT <= 16 )
		{
			const uint cellColorIndices = blockPosData[blockPosDataID+4];
			ProcessCellx16( cellColorIndices, cellParams, presenceBits, cellPosOffset );
		}
		else
		{
			const uint cellColorBucketCount = GRID_CELL_COUNT >> 4;			
			for ( uint cellColorBucket = 0; cellColorBucket < cellColorBucketCount; ++cellColorBucket )
			{
				const uint cellColorIndices = blockPosData[blockPosDataID+4+cellColorBucket];
				ProcessCellx16( cellColorIndices, cellParams, presenceBits, cellPosOffset );
			}
		}
	}
}


#endif  // #if BUFFER_WIDTH == 1
