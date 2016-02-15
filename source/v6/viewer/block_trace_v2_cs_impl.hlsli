#define HLSL

#include "common_shared.h"
#include "block_encoding.hlsli"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)

#if BLOCK_GET_STATS == 1 && HLSL_DEBUG_TRACE == 1
#define BLOCK_DEBUG_TRACE 1
#else
#define BLOCK_DEBUG_TRACE 0
#endif

Buffer< uint > blockPosData									: register( HLSL_TRACE_CULLED_BLOCK_SRV );
Buffer< uint > traceIndirectArgs							: register( HLSL_TRACE_INDIRECT_ARGS_SRV );
RWStructuredBuffer< BlockCellItem > blockCellItems			: register( HLSL_BLOCK_CELL_ITEM_UAV );
RWBuffer< uint > firstBlockCellItemIDs						: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV );
RWStructuredBuffer< BlockContext > blockContext				: register( HLSL_BLOCK_CONTEXT_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats		: register( HLSL_TRACE_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1
#if BLOCK_DEBUG_TRACE == 1
RWStructuredBuffer< DebugTraceV2 > debugTraces				: register( HLSL_TRACE_DEBUG_UAV );
#endif // #if BLOCK_DEBUG_TRACE == 1

struct CellParams
{
	uint	paletteColors[4];
	matrix	worldToProjMatrix;
	float3	rayOrgWS;
	float3	firstCellPosWS;
	float	cellSize;
	uint	blockPosDataID;
	uint	mip;

#if BLOCK_DEBUG_TRACE == 1
	uint				debugBlockID;
	DebugTraceBlock		debugTraceBlock;
#endif
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
	const uint multiSampledPixelWith = 2 * HLSL_PIXEL_SUPER_SAMPLING_WIDTH + 1;

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockTraceStats[0].cellProcessedCount, 1 );
	InterlockedAdd( blockTraceStats[0].sampleProcessedCount, multiSampledPixelWith * multiSampledPixelWith );
#endif // #if BLOCK_GET_STATS == 1

#if BLOCK_DEBUG_TRACE == 1
	const bool debug = cellParams.debugBlockID != (uint)-1;
	uint debugCellID = 0;
	DebugTraceCell debugCell = (DebugTraceCell)0;
	if ( debug )
		InterlockedAdd( blockContext[0].debugCellCount, 1, debugCellID );	
#endif // #if BLOCK_DEBUG_TRACE == 1

	float3 cellPosRS;
	uint pixelDepth23_none9;
	int2 multiSampledMinPixelCoords;	
	int2 minPixelCoords;

	{
		const uint cellX = (cellPos >> 0						) & HLSL_GRID_BLOCK_MASK;
		const uint cellY = (cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK;
		const uint cellZ = (cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK;
		const uint3 cellCoords = int3( cellX, cellY, cellZ );

		const float3 cellPosWS = mad( cellCoords, cellParams.cellSize, cellParams.firstCellPosWS );			
		const float4 cellPosCS = mul( cellParams.worldToProjMatrix, float4( cellPosWS, 1.0f ) );
		const float2 cellScreenPos = cellPosCS.xy * rcp( cellPosCS.w );		
		cellPosRS = cellPosWS - cellParams.rayOrgWS;
		pixelDepth23_none9 = uint( cellPosCS.w * c_blockGridScales[HLSL_MIP_MAX_COUNT-1].z ) << 9;

		const float2 multiSampledPixelPos = mad( cellScreenPos, 0.5f, 0.5f ) * c_blockMultiSampledFrameSize;
		multiSampledMinPixelCoords = int2( multiSampledPixelPos ) - HLSL_PIXEL_SUPER_SAMPLING_WIDTH;
		minPixelCoords = int2( (multiSampledMinPixelCoords + 0.5f) / float( HLSL_PIXEL_SUPER_SAMPLING_WIDTH ) );

#if BLOCK_DEBUG_TRACE == 1
		if ( debug )
		{
			debugCell.cellRGBA = cellRGBA;
			debugCell.cellPos = cellPos;
			debugCell.cellPosRS = cellPosRS;
			debugCell.pixelDepth23_none9 = pixelDepth23_none9;
			debugCell.multiSampledMinPixelCoords = multiSampledMinPixelCoords;	
			debugCell.cellX = cellX;
			debugCell.cellY = cellY;
			debugCell.cellZ = cellZ;			
			debugCell.cellCoords = cellCoords;
			debugCell.cellPosWS = cellPosWS;
			debugCell.cellPosCS = cellPosCS;
			debugCell.cellScreenPos = cellScreenPos;
			debugCell.multiSampledPixelPos = multiSampledPixelPos;
		}
#endif // #if BLOCK_DEBUG_TRACE == 1
	}
	
	uint hitBits[3] = { 0, 0, 0 };

	{
		const float rayEndVSX = mad( multiSampledMinPixelCoords.x + 0.5f, c_blockScreenToClipScale.x, c_blockScreenToClipOffset.x );
		const float rayEndVSY = mad( multiSampledMinPixelCoords.y + 0.5f, c_blockScreenToClipScale.y, c_blockScreenToClipOffset.y );
		const float3 rayEndVS0 = float3( rayEndVSX, rayEndVSY, -c_blockZNear );
		const float3 rayEndVS1 = float3( rayEndVSX + c_blockScreenToClipScale.x, rayEndVSY, -c_blockZNear );
		const float3 rayEndVS2 = float3( rayEndVSX, rayEndVSY  + c_blockScreenToClipScale.y, -c_blockZNear );
		const float3 rayEndWS0 = mul( c_blockViewToObject, float4( rayEndVS0, 1.0f ) ).xyz;
		const float3 rayEndWSR = mul( c_blockViewToObject, float4( rayEndVS1, 1.0f ) ).xyz - rayEndWS0;
		const float3 rayEndWSU = mul( c_blockViewToObject, float4( rayEndVS2, 1.0f ) ).xyz - rayEndWS0;
		const float3 rayEndRS0 = rayEndWS0 - cellParams.rayOrgWS;		
		
		const uint2 hitOffset = multiSampledMinPixelCoords - minPixelCoords * 3;
		const float cellScale = c_blockGridScales[cellParams.mip].y;

#if BLOCK_DEBUG_TRACE == 1
		if ( debug )
		{			
			debugCell.rayEndVSX = rayEndVSX;
			debugCell.rayEndVSY = rayEndVSY;
			debugCell.rayEndVS0 = rayEndVS0;
			debugCell.rayEndVS1 = rayEndVS1;
			debugCell.rayEndVS2 = rayEndVS2;
			debugCell.rayEndWS0 = rayEndWS0;
			debugCell.rayEndWSR = rayEndWSR;
			debugCell.rayEndWSU = rayEndWSU;
			debugCell.rayEndRS0 = rayEndRS0;
			debugCell.minPixelCoords = minPixelCoords;
			debugCell.hitOffset = hitOffset;
			debugCell.cellScale = cellScale;
		}
#endif // #if BLOCK_DEBUG_TRACE == 1

#if BLOCK_DEBUG_TRACE == 1
		DebugTraceCellSample debugTraceSamples[multiSampledPixelWith][multiSampledPixelWith];
#endif // #if BLOCK_DEBUG_TRACE == 1
		for ( uint y = 0; y < multiSampledPixelWith; ++y )
		{
			const uint hitY = y + hitOffset.y;
			const uint hitBucket = Div3( hitY );
			const uint hitYMod3_x_3 = Mod3( hitY ) * HLSL_PIXEL_SUPER_SAMPLING_WIDTH;

			float3 rayDir = rayEndRS0 + y * rayEndWSU;
			for ( uint x = 0; x < multiSampledPixelWith; ++x, rayDir += rayEndWSR )
			{
				const float3 rayInvDir = rcp( rayDir );
				const float3 offset = cellScale * rayInvDir;	
				const float3 t0 = mad( cellPosRS, rayInvDir, +offset );
				const float3 t1 = mad( cellPosRS, rayInvDir, -offset );
				const float3 tMin = min( t0, t1 );
				const float3 tMax = max( t0, t1 );
				const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
				const float tOut = min( min( tMax.x, tMax.y ), tMax.z );
				const uint hit = tIn <= tOut ? 1 : 0;

				const uint hitX = x + hitOffset.x;
				const uint hitShift = Div3( hitX ) * HLSL_PIXEL_SUPER_SAMPLING_WIDTH_SQ + hitYMod3_x_3 + Mod3( hitX );
				hitBits[hitBucket] |= hit << hitShift;

#if BLOCK_DEBUG_TRACE == 1
				if ( debug )
				{
					DebugTraceCellSample traceCellSample;
					traceCellSample.hit = hit;
					debugTraceSamples[y][x] = traceCellSample;
				}
#endif // #if BLOCK_DEBUG_TRACE == 1
			}
		}

#if BLOCK_DEBUG_TRACE == 1
		if ( debug )
			debugCell.samples = debugTraceSamples;
#endif // #if BLOCK_DEBUG_TRACE == 1
	}

	{
		const uint pixelOccupancyMask = (1 << (HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 3))-1;		
		const int2 frameSize = int2( c_blockFrameSize.xy );

#if BLOCK_DEBUG_TRACE == 1
		if ( debug )
		{
			debugCell.pixelOccupancyMask = pixelOccupancyMask;			
			debugCell.frameSize = frameSize;
		}
#endif // #if BLOCK_DEBUG_TRACE == 1

#if BLOCK_DEBUG_TRACE == 1
		DebugTraceCellGrid debugTraceGrids[3][3];
#endif // #if BLOCK_DEBUG_TRACE == 1
		for ( uint y = 0; y < 3; ++y )
		{
			for ( uint x = 0; x < 3; ++x )
			{
				const uint pixelOccupancy = (hitBits[y] >> (x * HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 3)) & pixelOccupancyMask;
				const int2 pixelCoords = minPixelCoords + uint2( x, y );

#if BLOCK_DEBUG_TRACE == 1
				DebugTraceCellGrid traceCellGrid = (DebugTraceCellGrid)0;
				if ( debug )
				{
					traceCellGrid.pixelOccupancy = pixelOccupancy;
					traceCellGrid.pixelCoords = pixelCoords;
				}
#endif // #if BLOCK_DEBUG_TRACE == 1

				if ( pixelOccupancy > 0 && pixelCoords.x >= 0 && pixelCoords.x < frameSize.x && pixelCoords.y >= 0 && pixelCoords.y < frameSize.y )
				{
					// optimization: use the LDS to blend for the block, will limit interlocked ops and number of exported cells
					uint blockCellItemID;
					InterlockedAdd( blockContext[0].cellItemCount, 1, blockCellItemID );
					blockCellItemID += 1;
						
					const uint pixelID = mad( pixelCoords.y, frameSize.x, pixelCoords.x );

					uint nextBlockCellItemID;
					InterlockedExchange( firstBlockCellItemIDs[pixelID], blockCellItemID, nextBlockCellItemID );
			
					BlockCellItem blockCellItem;
					blockCellItem.r8g8b8a8 = cellRGBA;
					blockCellItem.depth23_occupancy9 = pixelDepth23_none9 | pixelOccupancy;
					blockCellItem.nextID = nextBlockCellItemID;

					blockCellItems[blockCellItemID] = blockCellItem;

#if BLOCK_GET_STATS	== 1
					InterlockedAdd( blockTraceStats[0].pixelSampleCount, countbits( pixelOccupancy ) );
#endif // #if BLOCK_GET_STATS == 1

#if BLOCK_DEBUG_TRACE == 1
					if ( debug )
					{
						traceCellGrid.blockCellItemID = blockCellItemID;						
						traceCellGrid.nextBlockCellItemID = nextBlockCellItemID;
					}
#endif // #if BLOCK_DEBUG_TRACE == 1
				}
#if BLOCK_DEBUG_TRACE == 1
				if ( debug )
					debugTraceGrids[y][x] = traceCellGrid;
#endif // #if BLOCK_DEBUG_TRACE == 1
			}
		}

#if BLOCK_DEBUG_TRACE == 1
		if ( debug )
			debugCell.grids = debugTraceGrids;
#endif // #if BLOCK_DEBUG_TRACE == 1
	}

#if BLOCK_DEBUG_TRACE == 1
	if ( debug )
	{
		debugTraces[debugCellID].block = cellParams.debugTraceBlock;
		debugTraces[debugCellID].cell = debugCell;
	}
#endif // #if BLOCK_DEBUG_TRACE == 1
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

#if BLOCK_DEBUG_TRACE == 1
		const bool debug = blockID == 0 && GRID_CELL_BUCKET == 2;
		uint debugBlockID = 0;
		if ( debug )
			InterlockedAdd( blockContext[0].debugBlockCount, 1, debugBlockID );	
#endif // #if BLOCK_DEBUG_TRACE == 1


		CellParams cellParams = (CellParams)0;
		cellParams.blockPosDataID = blockPosDataID;
		cellParams.mip = mip;
		cellParams.cellSize = gridScale * HLSL_GRID_INV_WIDTH * 2.0f;
		cellParams.firstCellPosWS = mad( blockCoords, cellParams.cellSize, -gridScale + cellParams.cellSize * 0.5f ) + c_blockCenter;
		cellParams.rayOrgWS = float3( c_blockViewToObject[0].w, c_blockViewToObject[1].w, c_blockViewToObject[2].w );
		cellParams.worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );
		Block_DecodeColors( endPointColors, cellParams.paletteColors );
		
#if BLOCK_DEBUG_TRACE == 1
		if ( debug )
		{			
			DebugTraceBlock traceBlock;
			traceBlock.blockSize = blockSize;	
			traceBlock.blockPosDataID = blockPosDataID;
			traceBlock.packedBlockPos = packedBlockPos;
			traceBlock.endPointColors = endPointColors;
			traceBlock.mip = mip;
			traceBlock.blockPos = blockPos;
			traceBlock.blockX = blockX;
			traceBlock.blockY = blockY;
			traceBlock.blockZ = blockZ;
			traceBlock.blockCoords = blockCoords;
			traceBlock.gridScale = gridScale;			
			traceBlock.cellParams_paletteColors = cellParams.paletteColors;
			traceBlock.cellParams_worldToProjMatrix = cellParams.worldToProjMatrix;
			traceBlock.cellParams_rayOrgWS = cellParams.rayOrgWS;
			traceBlock.cellParams_firstCellPosWS = cellParams.firstCellPosWS;
			traceBlock.cellParams_cellSize = cellParams.cellSize;
			traceBlock.cellParams_blockPosDataID = cellParams.blockPosDataID;
			traceBlock.cellParams_mip = cellParams.mip;
			cellParams.debugBlockID = debugBlockID;
			cellParams.debugTraceBlock = traceBlock;
		}
		else
		{
			cellParams.debugBlockID = (uint)-1;
		}
#endif // #if BLOCK_DEBUG_TRACE == 1

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
