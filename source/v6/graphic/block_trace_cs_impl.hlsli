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
	const float2 frameSize = c_traceFrameSize.xy;

	const float2 otherPixelCoords = pixelCoords + float2( x, y );
	const float3 rayDir = mad( otherPixelCoords.y, c_traceEyes[eye].rayDirUp, mad( otherPixelCoords.x, c_traceEyes[eye].rayDirRight, c_traceEyes[eye].rayDirBase ) );
	const float3 rayInvDir = rcp( rayDir );
	const float3 t0 = boxMinRS * rayInvDir;
	const float3 t1 = boxMaxRS * rayInvDir;
	// optimization: direction is know, t0 and t1 could be ordered statically
	const float3 tMin = min( t0, t1 );
	const float3 tMax = max( t0, t1 );
	const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
	const float tOut = min( min( tMax.x, tMax.y ), tMax.z );
	const bool hit = (tIn <= tOut) && IsValidCoords( otherPixelCoords );

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
		float2 curPixelCoords[2];

		{
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
			// optimization: do everything in camera relative space
			// optimization: add bias inside grid center
			const float3 cellPosWS = mad( cellCoords, halfCellSize * 2.0, -gridScale + halfCellSize ) + c_traceGridCenters[mip].xyz;

			for ( uint eye = 0; eye < c_traceEyeCount; ++eye )
			{
				{
					// optimization: do everything in camera relative space
					const float3 cellPosRS = cellPosWS - c_traceEyes[eye].org; 
					boxMinRS[eye] = cellPosRS - halfCellSize;
					boxMaxRS[eye] = cellPosRS + halfCellSize;
				}

				float2 prevPixelUV;

				{
					float4 cellPosCS;
					cellPosCS.x = mul( c_traceEyes[eye].prevWorldToProj[0].xyz, cellPosWS ) + c_traceEyes[eye].prevWorldToProj[0].w;
					cellPosCS.y = mul( c_traceEyes[eye].prevWorldToProj[1].xyz, cellPosWS ) + c_traceEyes[eye].prevWorldToProj[1].w;
					cellPosCS.w = mul( c_traceEyes[eye].prevWorldToProj[3].xyz, cellPosWS ) + c_traceEyes[eye].prevWorldToProj[3].w;
					const float2 cellScreenPos = cellPosCS.xy * rcp( cellPosCS.w );
			
					prevPixelUV = mad( cellScreenPos, 0.5f, 0.5f );
				}

				{
					float4 cellPosCS;
					cellPosCS.x = mul( c_traceEyes[eye].curWorldToProj[0].xyz, cellPosWS ) + c_traceEyes[eye].curWorldToProj[0].w;
					cellPosCS.y = mul( c_traceEyes[eye].curWorldToProj[1].xyz, cellPosWS ) + c_traceEyes[eye].curWorldToProj[1].w;
					cellPosCS.w = mul( c_traceEyes[eye].curWorldToProj[3].xyz, cellPosWS ) + c_traceEyes[eye].curWorldToProj[3].w;
					const float2 cellScreenPos = cellPosCS.xy * rcp( cellPosCS.w );
			
					pixelDepth[eye] = cellPosCS.w;

					const float2 curPixelUV = mad( cellScreenPos, 0.5f, 0.5f );
					pixelDisplacementsF16[eye] = f32tof16( curPixelUV - prevPixelUV );
					curPixelCoords[eye] = floor( curPixelUV * c_traceFrameSize ) + c_traceJitter;
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
				if ( IsValidCoords( curPixelCoords[eye] ) )
				{
					uint hitMask9 = 0;
					{
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye], -1, -1, boxMinRS[eye], boxMaxRS[eye], eye ) << 0;
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye],  0, -1, boxMinRS[eye], boxMaxRS[eye], eye ) << 1;
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye], +1, -1, boxMinRS[eye], boxMaxRS[eye], eye ) << 2;

						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye], -1,  0, boxMinRS[eye], boxMaxRS[eye], eye ) << 3;
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye],  0,  0, boxMinRS[eye], boxMaxRS[eye], eye ) << 4;
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye], +1,  0, boxMinRS[eye], boxMaxRS[eye], eye ) << 5;
						
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye], -1, +1, boxMinRS[eye], boxMaxRS[eye], eye ) << 6;
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye],  0, +1, boxMinRS[eye], boxMaxRS[eye], eye ) << 7;
						hitMask9 |= TraceCell( blockCellID, curPixelCoords[eye], +1, +1, boxMinRS[eye], boxMaxRS[eye], eye ) << 8;
					}

					{
						const int2 frameSize = int2( c_traceFrameSize.xy );
						const int cellItemCountPerPage = frameSize.x * c_traceEyeCount * frameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
						const uint2 cellItemPixelCoords = uint2( curPixelCoords[eye].x + eye * frameSize.x, curPixelCoords[eye].y );
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
								InterlockedAdd( blockTraceStats[0].pixelSampleCount, countbits( hitMask9 ) );
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
		}
	}
}
