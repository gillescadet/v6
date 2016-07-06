#define HLSL

#include "trace_shared.h"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

Buffer< uint > blockData										: REGISTER_SRV( HLSL_BLOCK_DATA_SLOT );
Buffer< uint > traceCells										: REGISTER_SRV( HLSL_TRACE_CELLS_SLOT );
Buffer< uint > traceIndirectArgs								: REGISTER_SRV( HLSL_TRACE_INDIRECT_ARGS_SLOT );

RWStructuredBuffer< BlockCellItem > blockCellItems				: REGISTER_UAV( HLSL_BLOCK_CELL_ITEM_SLOT );
RWBuffer< uint > blockCellItemCounters							: REGISTER_UAV( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT );
#if BLOCK_DEBUG == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats			: REGISTER_UAV( HLSL_TRACE_STATS_SLOT );
RWStructuredBuffer< BlockTraceCellStats > blockTraceCellStats	: REGISTER_UAV( HLSL_TRACE_CELL_STATS_SLOT );
#endif // #if BLOCK_DEBUG == 1

bool IsValidCoords( float2 pixelCoords )
{
	const float2 frameSize = c_traceFrameSize.xy;

	return pixelCoords.x >= 0 && pixelCoords.x < frameSize.x && pixelCoords.y >= 0 && pixelCoords.y < frameSize.y;
}

bool TraceCell( uint blockCellID, float2 pixelCoords, float x, float y, float3 boxMinRS, float3 boxMaxRS, uint eye )
{
	// optimization: if direction is known, t0 and t1 could be ordered statically

	const float2 frameSize = c_traceFrameSize.xy;
																																											// | and | cmpf | addf | mulf | madf | minf | maxf | divf |
	const float2 otherPixelCoords = pixelCoords + float2( x, y );																											// |     |      |    2 |      |      |      |      |      |
	const float3 rayDir = mad( otherPixelCoords.y, c_traceEyes[eye].rayDirUp, mad( otherPixelCoords.x, c_traceEyes[eye].rayDirRight, c_traceEyes[eye].rayDirBase ) );		// |     |      |      |      |    6 |      |      |      |
	const float3 rayInvDir = rcp( rayDir );																																	// |     |      |      |      |      |      |      |    3 |
	const float3 t0 = boxMinRS * rayInvDir;																																	// |     |      |      |    3 |      |      |      |      |
	const float3 t1 = boxMaxRS * rayInvDir;																																	// |     |      |      |    3 |      |      |      |      |
	const float3 tMin = min( t0, t1 );																																		// |     |      |      |      |      |    3 |      |      |
	const float3 tMax = max( t0, t1 );																																		// |     |      |      |      |      |      |    3 |      |
	const float tIn = max( max( tMin.x, tMin.y ), tMin.z );																													// |     |      |      |      |      |      |    2 |      |
	const float tOut = min( min( tMax.x, tMax.y ), tMax.z );																												// |     |      |      |      |      |    2 |      |      |
	const bool hit = (tIn <= tOut) && IsValidCoords( otherPixelCoords );																									// |   4 |    5 |      |      |      |      |      |      |
																																											// |-----|------|------|------|------|------|------|------|
																																											// |   4 |    5 |    2 |    6 |    6 |    5 |    5 |    3 |
																																											// => 45 cycles
#if BLOCK_DEBUG == 1
	if ( c_traceGetStats )
	{
		uint traceCellStatID;
		InterlockedAdd( blockTraceStats[0].traceCellStatCount, 1, traceCellStatID );
		if ( traceCellStatID < HLSL_BLOCK_TRACE_CELL_STATS_MAX_COUNT )
		{
			BlockTraceCellStats cellStats;
			cellStats.blockCellID = blockCellID;
			cellStats.pixelCoords = pixelCoords;
			cellStats.x = x;
			cellStats.y = y;
			cellStats.boxMinRS = boxMinRS;
			cellStats.boxMaxRS = boxMaxRS;
			cellStats.rayDir = rayDir;
			cellStats.tIn = tIn;
			cellStats.tOut = tOut;
			cellStats.hit = hit;

			blockTraceCellStats[traceCellStatID] = cellStats;
		}
	}
#endif
	return hit;
}

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint cellID = DTid.x;

#if BLOCK_DEBUG == 1
	if ( c_traceGetStats )
		InterlockedAdd( blockTraceStats[0].cellInputCount, 1 );
#endif // #if BLOCK_DEBUG == 1

	if ( cellID < trace_blockCount( GRID_CELL_BUCKET ) * GRID_CELL_COUNT )
	{
		uint blockCellID;
		bool valid;
		uint mip;
		uint rgb_none;
		float3 boxMinRS[2];
		float3 boxMaxRS[2];
		float pixelDepth[2];
		uint2 pixelDisplacementsF16[2];
		float2 curPixelJitteredCoords[2];
		float2 curPixelSides[2];

		{
			// 550 us (0 us)

			const uint blockRank = cellID >> GRID_CELL_SHIFT;
			const uint cellRank = cellID & GRID_CELL_MASK;

			const uint traceBlockOffset = trace_blockOffset( GRID_CELL_BUCKET );
			const uint traceBlockID = traceBlockOffset + blockRank;
			const uint packedPos = traceCells[traceBlockID * 2 + 0];
#if BLOCK_DEBUG == 1
			uint blockDataID = traceCells[traceBlockID * 2 + 1];
			const uint historyColor = blockDataID >> 30;
			blockDataID &= 0x3FFFFFFF;
#else
			const uint blockDataID = traceCells[traceBlockID * 2 + 1];
#endif
			blockCellID = blockDataID + cellRank;
			const uint packedColor = blockData[blockCellID];

			valid = packedColor != HLSL_GRID_BLOCK_CELL_EMPTY;

			mip = packedPos >> 28;
			const uint blockPos = packedPos & 0x0FFFFFFF;
			rgb_none = packedColor & ~0xFF;
			const uint cellPos = packedColor & 0x3F;
			const uint gridMacroMask = (1 << c_cullGridMacroShift)-1;
			const uint x = (((blockPos >> (c_traceGridMacroShift*0)) & gridMacroMask) << 2) | ((cellPos >> 0) & 3);
			const uint y = (((blockPos >> (c_traceGridMacroShift*1)) & gridMacroMask) << 2) | ((cellPos >> 2) & 3);
			const uint z = (((blockPos >> (c_traceGridMacroShift*2)) & gridMacroMask) << 2) | ((cellPos >> 4) & 3);
			const uint3 cellCoords = uint3( x, y, z );

#if BLOCK_DEBUG == 1
			if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_MIPS )
			{
				const uint mipColors[6] = { 0xFF000000, 0x00FF0000, 0x0000FF00, 0x7F000000, 0x007F0000, 0x00007F00 };
				rgb_none = mipColors[mip % 6];
			}
			else if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_BUCKETS )
			{
				const uint bucketColors[5] = { 0xFF000000, 0x00FF0000, 0x0000FF00, 0xFF00FF00, 0xFFFF0000 };
				rgb_none = bucketColors[GRID_CELL_BUCKET];
			}
			else if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_HISTORY )
			{
				const uint historyColors[4] = { 0xFF000000, 0x00FF0000, 0x0000FF00, 0x7F7F7F00 };
				rgb_none = historyColors[historyColor];
			}
#endif // #if BLOCK_DEBUG == 1

			const float gridScale = c_traceGridScales[mip].x;
			const float halfCellSize = c_traceGridScales[mip].y;
			const float3 cellCenterWS = mad( cellCoords, halfCellSize * 2.0, -gridScale + halfCellSize ) + c_traceGridCenters[mip].xyz;

			// 1000 us (estimation)

			for ( uint eye = 0; eye < c_traceEyeCount; ++eye )
			{
				{
					const float3 cellCenterRS = cellCenterWS - c_traceEyes[eye].org; 
					boxMinRS[eye] = cellCenterRS - halfCellSize;
					boxMaxRS[eye] = cellCenterRS + halfCellSize;
				}

				float2 prevPixelCoords;
				{
					// 200 us

					float4 prevCellPosCS;
					prevCellPosCS.x = dot( c_traceEyes[eye].prevWorldToProjX.xyz, cellCenterWS ) + c_traceEyes[eye].prevWorldToProjX.w;
					prevCellPosCS.y = dot( c_traceEyes[eye].prevWorldToProjY.xyz, cellCenterWS ) + c_traceEyes[eye].prevWorldToProjY.w;
					prevCellPosCS.w = dot( c_traceEyes[eye].prevWorldToProjW.xyz, cellCenterWS ) + c_traceEyes[eye].prevWorldToProjW.w;
					const float2 prevCellScreenPos = prevCellPosCS.xy * rcp( prevCellPosCS.w );
			
					const float2 prevPixelUV = mad( prevCellScreenPos, 0.5f, 0.5f );
					prevPixelCoords = prevPixelUV * c_traceFrameSize;
				}

				{
					// 250 us (estimation)

					float4 curCellPosCS;
					curCellPosCS.x = dot( c_traceEyes[eye].curWorldToProjX.xyz, cellCenterWS ) + c_traceEyes[eye].curWorldToProjX.w;
					curCellPosCS.y = dot( c_traceEyes[eye].curWorldToProjY.xyz, cellCenterWS ) + c_traceEyes[eye].curWorldToProjY.w;
					curCellPosCS.w = dot( c_traceEyes[eye].curWorldToProjW.xyz, cellCenterWS ) + c_traceEyes[eye].curWorldToProjW.w;
					const float2 curCellScreenPos = curCellPosCS.xy * rcp( curCellPosCS.w );
			
					pixelDepth[eye] = curCellPosCS.w;

					const float2 curPixelUV = mad( curCellScreenPos, 0.5f, 0.5f );
					const float2 curPixelCoords = curPixelUV * c_traceFrameSize;
					pixelDisplacementsF16[eye] = f32tof16( curPixelCoords - prevPixelCoords );
					curPixelJitteredCoords[eye] = floor( curPixelCoords ) + c_traceJitter;
					curPixelSides[eye].x = curPixelCoords.x < curPixelJitteredCoords[eye].x ? -1.0f : 1.0f;
					curPixelSides[eye].y = curPixelCoords.y < curPixelJitteredCoords[eye].y ? -1.0f : 1.0f;
				}
			}
		}

#if BLOCK_DEBUG == 1
		if ( c_traceGetStats )
			InterlockedAdd( blockTraceStats[0].cellProcessedCounts[mip], 1 );
#endif // #if BLOCK_DEBUG == 1

		if ( valid )
		{
			for ( uint eye = 0; eye < c_traceEyeCount; ++eye )
			{
				if ( IsValidCoords( curPixelJitteredCoords[eye] ) )
				{
					// 2880 us (5540 us)

					uint hitMask9 = 0;

					{
						float2 offsets[4];
						offsets[0] = float2( 0.0f,                 0.0f );
						offsets[1] = float2( curPixelSides[eye].x, 0.0f );
						offsets[2] = float2( 0.0f,                 curPixelSides[eye].y );
						offsets[3] = float2( curPixelSides[eye].x, curPixelSides[eye].y );

						hitMask9 |= TraceCell( blockCellID, curPixelJitteredCoords[eye], offsets[0].x, offsets[0].y, boxMinRS[eye], boxMaxRS[eye], eye ) << uint( offsets[0].x + (offsets[0].y * 3.0f + 4.0f) );
						hitMask9 |= TraceCell( blockCellID, curPixelJitteredCoords[eye], offsets[1].x, offsets[1].y, boxMinRS[eye], boxMaxRS[eye], eye ) << uint( offsets[1].x + (offsets[1].y * 3.0f + 4.0f) );
						hitMask9 |= TraceCell( blockCellID, curPixelJitteredCoords[eye], offsets[2].x, offsets[2].y, boxMinRS[eye], boxMaxRS[eye], eye ) << uint( offsets[2].x + (offsets[2].y * 3.0f + 4.0f) );
						hitMask9 |= TraceCell( blockCellID, curPixelJitteredCoords[eye], offsets[3].x, offsets[3].y, boxMinRS[eye], boxMaxRS[eye], eye ) << uint( offsets[3].x + (offsets[3].y * 3.0f + 4.0f) );
					}

					// 540 us (6080 us)

					if ( hitMask9 != 0 )
					{
						const int2 frameSize = int2( c_traceFrameSize.xy );
						const int cellItemCountPerPage = frameSize.x * c_traceEyeCount * frameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
						const uint2 cellItemPixelCoords = uint2( curPixelJitteredCoords[eye].x + eye * frameSize.x, curPixelJitteredCoords[eye].y );
						const uint pixelID = (mad( cellItemPixelCoords.y >> 3, (frameSize.x * c_traceEyeCount) >> 3, cellItemPixelCoords.x >> 3 ) << 6) + mad( cellItemPixelCoords.y & 7, 8, cellItemPixelCoords.x & 7 );

						uint blockCellItemRank = 0;
						InterlockedAdd( blockCellItemCounters[pixelID], 1, blockCellItemRank );

						if ( blockCellItemRank < HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT )
						{
							const uint blockCellItemPage = blockCellItemRank >> HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT;
							const uint blockCellItemRankInPage = blockCellItemRank & HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK;
							const uint blockCellItemID = blockCellItemPage * cellItemCountPerPage + HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * pixelID + blockCellItemRankInPage;
					
							blockCellItems[blockCellItemID].hitMask1_depth31 = ((hitMask9 & 0x100) << 23) | asuint( pixelDepth[eye] );
							blockCellItems[blockCellItemID].r8g8b8_hitMask8 = rgb_none | (hitMask9 & 0xFF);
							blockCellItems[blockCellItemID].xdsp16_ydsp16 = (pixelDisplacementsF16[eye].x << 16) | pixelDisplacementsF16[eye].y;

#if BLOCK_DEBUG == 1
							if ( c_traceGetStats )
							{
								const uint pixelSampleCount = countbits( hitMask9 );
								InterlockedAdd( blockTraceStats[0].pixelSampleCount, pixelSampleCount );
								InterlockedAdd( blockTraceStats[0].pixelSampleDistribution[pixelSampleCount], 1 );
								InterlockedAdd( blockTraceStats[0].cellItemCounts[mip], 1 );
							}
#endif // #if BLOCK_DEBUG == 1
						}

#if BLOCK_DEBUG == 1
						if ( c_traceGetStats )
							InterlockedMax( blockTraceStats[0].cellItemMaxCountPerPixel, blockCellItemRank );
#endif // #if BLOCK_DEBUG == 1
					}
				}
			}

#if BLOCK_DEBUG == 1
			if ( c_traceGetStats )
				InterlockedAdd( blockTraceStats[0].cellValidCount, 1 );
#endif // #if BLOCK_DEBUG == 1
		}
	}
}
