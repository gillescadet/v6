#define HLSL

#include "common_shared.h"

Buffer< uint > blockColors									: register( HLSL_TRACE_CULLED_BLOCK_SRV );
#include "block_cell.hlsli"

#define BUFFER_WIDTH HLSL_CELL_SUPER_SAMPLING_WIDTH

Buffer< uint > traceIndirectArgs							: register( HLSL_TRACE_INDIRECT_ARGS_SRV );
RWStructuredBuffer< BlockCellItem > blockCellItems			: register( HLSL_BLOCK_CELL_ITEM_UAV );
RWBuffer< uint > firstBlockCellItemIDs						: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV );
RWStructuredBuffer< BlockContext > blockContext				: register( HLSL_BLOCK_CONTEXT_UAV );
#if BLOCK_GET_STATS == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats		: register( HLSL_TRACE_STATS_UAV );
#endif // #if BLOCK_GET_STATS == 1
#if HLSL_DEBUG_BLOCK == 1
RWStructuredBuffer< DebugBlock > debugBlocks				: register( HLSL_BLOCK_DEBUG_UAV );
#endif // #if HLSL_DEBUG_BLOCK == 1
#if HLSL_DEBUG_PIXEL == 1
RWStructuredBuffer< DebugTrace > debugTraces				: register( HLSL_TRACE_DEBUG_UAV );
#endif // #if HLSL_DEBUG_PIXEL == 1

struct TraceBlock_s
{
	float3	center;
	uint	occupancy;
	uint	multiSampledMinPixel_basex14y14_offsetx2y2;
	uint	debug4_jobWidth12_jobOffset12_mip4;
};

#if HLSL_CELL_SUPER_SAMPLING_WIDTH == 3
static const uint			pixelCountMax = 49;
static const uint			traceCellWidthPerBlock = 3;
static const uint			traceJobPerInt = 4;
#endif

groupshared TraceBlock_s	g_blocks[HLSL_BLOCK_THREAD_GROUP_SIZE];
groupshared uint			g_traceCellBits[HLSL_BLOCK_THREAD_GROUP_SIZE * traceCellWidthPerBlock];
groupshared uint			g_jobs[(HLSL_BLOCK_THREAD_GROUP_SIZE * pixelCountMax + traceJobPerInt - 1) / traceJobPerInt];
groupshared uint			g_jobCount = 0;
#if HLSL_DEBUG_BLOCK == 1
groupshared int				g_debugBlockID = -1;
#endif // #if HLSL_DEBUG_BLOCK == 1

void ParallelInit( uint blockID )
{
	g_jobCount == 0;

	const uint bucketBase = blockID * traceCellWidthPerBlock;
	for ( uint bucketOffset = 0; bucketOffset < traceCellWidthPerBlock; ++bucketOffset )
		g_traceCellBits[bucketBase + bucketOffset] = 0;	
}

bool Clip( BlockCell blockCell, out uint2 multiSampledMinPixelCoords, out uint2 multiSampledMaxPixelCoords, out float pixelDepth, uint packedID, bool debug, out bool debugBlock )
{
	const matrix worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );
	
	uint3 boxMin = uint3( 2, 2, 2 );
	uint3 boxMax = uint3( 0, 0, 0 );
		
	{
		uint occupancyBit = 0;
		for ( uint z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
		{
			for ( uint y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
			{
				for ( uint x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++occupancyBit )
				{
					if ( (blockCell.occupancy & (1 << occupancyBit)) != 0 )
					{
						const uint3 p = uint3( x, y, z );
						boxMin = min( boxMin, p );
						boxMax = max( boxMax, p );
					}
				}
			}
		}
	}

	const float scale = 2.0f * blockCell.halfCellSize / 3.0f; 
	float3 pointMin = boxMin * scale;
	float3 pointMax = (boxMax + 1.0f) * scale;
	float3 delta = pointMax - pointMin;
	pointMin += blockCell.posWS - blockCell.halfCellSize;

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

	float2 minScreenPos = float2(  1e32f,  1e32f );
	float2 maxScreenPos = float2( -1e32f, -1e32f );
	
	uint clippedCount = 0;
	pixelDepth = 1e32f;
	for ( uint vertexID = 0; vertexID < 8; ++vertexID )
	{
		const float3 posWS = pointMin + delta * vertices[vertexID];
		const float4 posCS = mul( worldToProjMatrix, float4( posWS, 1.0f ) );
		const float2 screenPos = posCS.xy * rcp( posCS.w );		
		minScreenPos = min( minScreenPos, screenPos );
		maxScreenPos = max( maxScreenPos, screenPos );
		pixelDepth = min( pixelDepth, posCS.w );
		clippedCount += (abs( posCS.x ) > posCS.w || abs( posCS.y ) > posCS.w || posCS.w < 0) ? 1 : 0;
	}
	
	const float2 screenPos = (minScreenPos + maxScreenPos) * 0.5f;
	const float2 screenRadius = (maxScreenPos - minScreenPos) * 0.5f;
	const float2 multiSampledPixelPos = mad( screenPos, 0.5f, 0.5f ) * c_blockMultiSampledFrameSize;
	const float2 multiSampledPixelRadius = min( screenRadius * 0.5f * c_blockMultiSampledFrameSize, HLSL_CELL_SUPER_SAMPLING_WIDTH );
	multiSampledMinPixelCoords = clamp( int2( multiSampledPixelPos - multiSampledPixelRadius ), 0, c_blockMultiSampledFrameSize-1 );
	multiSampledMaxPixelCoords = clamp( int2( multiSampledPixelPos + multiSampledPixelRadius ), 0, c_blockMultiSampledFrameSize-1 );	

	debugBlock = false;
#if HLSL_DEBUG_BLOCK == 1
	if ( debug )
	{
		blockContext[0].screenPos					= screenPos;
		blockContext[0].screenRadius				= screenRadius;
		blockContext[0].multiSampledPixelPos		= multiSampledPixelPos;
		blockContext[0].multiSampledPixelRadius		= multiSampledPixelRadius;
		blockContext[0].multiSampledMinPixelCoords	= multiSampledMinPixelCoords;
		blockContext[0].multiSampledMaxPixelCoords	= multiSampledMaxPixelCoords;		
	}

#if HLSL_DEBUG_PIXEL == 1
	const uint2 pixelPos = mad( screenPos, 0.5f, 0.5f ) * c_blockFrameSize;
	debugBlock = c_blockDebug != 0 && (c_blockDebugCoords.x >> 3) == (pixelPos.x >> 3) && (c_blockDebugCoords.y >> 3) == ((((uint)c_blockFrameSize.y - pixelPos.y - 1) ) >> 3);
#endif // #if HLSL_DEBUG_PIXEL == 1

#endif // #if HLSL_DEBUG_BLOCK == 1

	return clippedCount == 8;
}

void Assign( BlockCell blockCell, uint blockID, uint2 multiSampledMinPixelCoords, uint2 multiSampledMaxPixelCoords, out uint2 minPixelCoords, bool debug, bool debugBlock )
{
	minPixelCoords = uint2( (multiSampledMinPixelCoords + 0.5f) / float( HLSL_CELL_SUPER_SAMPLING_WIDTH ) );
	const uint2 multiSampledMinPixelBase = minPixelCoords * HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const uint2 multiSampledMinPixelOffset = multiSampledMinPixelCoords - multiSampledMinPixelBase;	
	const uint2 multiSampledSize = 1 + multiSampledMaxPixelCoords - multiSampledMinPixelCoords;
	const uint multiSampledPixelCount = multiSampledSize.x * multiSampledSize.y;

	uint jobOffset;
	InterlockedAdd( g_jobCount, multiSampledPixelCount, jobOffset );

#if HLSL_DEBUG_BLOCK == 1
	if ( debug )
	{
		blockContext[0].minPixelCoords				= minPixelCoords;
		blockContext[0].multiSampledMinPixelBase	= multiSampledMinPixelBase;
		blockContext[0].multiSampledMinPixelOffset	= multiSampledMinPixelOffset;
		blockContext[0].multiSampledSize			= multiSampledSize;
		blockContext[0].multiSampledPixelCount		= multiSampledPixelCount;
		blockContext[0].jobCount					= jobOffset + multiSampledPixelCount;
	}

#endif // #if HLSL_DEBUG_BLOCK == 1

	TraceBlock_s block;
	block.center = blockCell.posWS;
	block.occupancy = blockCell.occupancy;
	block.multiSampledMinPixel_basex14y14_offsetx2y2 = (multiSampledMinPixelBase.x << 18) | (multiSampledMinPixelBase.y << 4) | (multiSampledMinPixelOffset.x << 2) | multiSampledMinPixelOffset.y;
	block.debug4_jobWidth12_jobOffset12_mip4 = (debugBlock ? 0x80000000 : 0) | (multiSampledSize.x << 16) | (jobOffset << 4) | blockCell.mip;
	g_blocks[blockID] = block;
	
	for ( uint jobRank = 0; jobRank < multiSampledPixelCount; ++jobRank )
	{
		const uint jobID = jobOffset + jobRank;
		const uint jobBucket = jobID >> 2;
		const uint jobShift = (jobID&3) * 8;
		InterlockedAnd( g_jobs[jobBucket], ~(0xFF << jobShift) );
		InterlockedOr( g_jobs[jobBucket], blockID << jobShift );
	}

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockTraceStats[0].blockProcessedCount, 1 );
	InterlockedAdd( blockTraceStats[0].traceInputCount, multiSampledPixelCount );
#endif // #if BLOCK_GET_STATS == 1
}

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

void ParallelTrace( uint blockID )
{
	const float3 rayOrgWS = float3( c_blockViewToObject[0].w, c_blockViewToObject[1].w, c_blockViewToObject[2].w );

	for ( uint jobID = blockID; jobID < g_jobCount; jobID += HLSL_BLOCK_THREAD_GROUP_SIZE )
	{
		const uint jobBucket = jobID >> 2;
		const uint jobShift = (jobID&3) * 8;
		const uint jobBlockID = (g_jobs[jobBucket] >> jobShift) & 0xFF;
		const TraceBlock_s block = g_blocks[jobBlockID];
#if HLSL_DEBUG_PIXEL == 1
		const bool jobDebugBlock = (block.debug4_jobWidth12_jobOffset12_mip4 & 0x80000000) != 0;
#endif
		const uint jobWidth = (block.debug4_jobWidth12_jobOffset12_mip4 >> 16) & 0xFFF;
		const uint jobOffset = (block.debug4_jobWidth12_jobOffset12_mip4 >> 4) & 0xFFF;
		const uint jobMip = block.debug4_jobWidth12_jobOffset12_mip4 & 0xF;
		const uint jobPixelID = jobID - jobOffset;
		const uint jobPixelBaseX = block.multiSampledMinPixel_basex14y14_offsetx2y2 >> 18;
		const uint jobPixelBaseY = (block.multiSampledMinPixel_basex14y14_offsetx2y2 >> 4) & 0x3FFF;
		const uint jobPixelOffsetX = (block.multiSampledMinPixel_basex14y14_offsetx2y2 >> 2) & 3;
		const uint jobPixelOffsetY = block.multiSampledMinPixel_basex14y14_offsetx2y2 & 3;
		const float lineCount = trunc( (jobPixelID + 0.5f) * rcp( jobWidth ) );
		const float jobPixelX = jobPixelID - mad( lineCount, jobWidth, -0.5f );
		const float jobPixelY = lineCount + 0.5f;		
		
		const float2 jobMultiSampledScreenPos = float2( jobPixelBaseX + jobPixelOffsetX + jobPixelX, jobPixelBaseY + jobPixelOffsetY + jobPixelY );
		const float3 rayEndVS = float3( mad( jobMultiSampledScreenPos, c_blockMultiSampledScreenToClipScale, c_blockScreenToClipOffset ), -c_blockZNear );
		const float3 rayEndWS = mul( c_blockViewToObject, float4( rayEndVS, 1.0f ) ).xyz;
		const float3 rayDir = normalize( rayEndWS - rayOrgWS );
		const float3 rayInvDir = rcp( rayDir );
		const float3 alpha = (block.center - rayOrgWS) * rayInvDir;
		const float3 beta = c_blockGridScales[jobMip].y * rayInvDir;	
		const float3 t0 = alpha + beta;
		const float3 t1 = alpha - beta;
		const float3 tMin = min( t0, t1 );
		const float3 tMax = max( t0, t1 );
		const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
		const float tOut = min( min( tMax.x, tMax.y ), tMax.z );

#if HLSL_DEBUG_BLOCK == 1
		const bool debug = uint( g_debugBlockID ) == jobBlockID && jobPixelID == 1;
		if ( debug )
		{
			blockContext[0].jobBlockID					= jobBlockID;
			blockContext[0].jobWidth					= jobWidth;
			blockContext[0].jobOffset					= jobOffset;
			blockContext[0].jobMip						= jobMip;
			blockContext[0].jobPixelID					= jobPixelID;
			blockContext[0].jobPixelBaseX				= jobPixelBaseX;
			blockContext[0].jobPixelBaseY				= jobPixelBaseY;
			blockContext[0].jobPixelOffsetX				= jobPixelOffsetX;
			blockContext[0].jobPixelOffsetY				= jobPixelOffsetY;
			blockContext[0].lineCount					= lineCount;
			blockContext[0].jobPixelX					= jobPixelX;
			blockContext[0].jobPixelY					= jobPixelY;

			blockContext[0].gridCenter					= block.center;
			blockContext[0].gridScale					= c_blockGridScales[jobMip].y;
			blockContext[0].gridOccupancy				= block.occupancy;
			
			blockContext[0].jobMultiSampledScreenPos	= jobMultiSampledScreenPos;
			blockContext[0].rayEndVS					= rayEndVS;
			blockContext[0].rayOrgWS					= rayOrgWS;
			blockContext[0].rayEndWS					= rayEndWS;
			blockContext[0].rayDir						= rayDir;
			blockContext[0].rayInvDir					= rayInvDir;
			blockContext[0].alpha						= alpha;
			blockContext[0].beta						= beta;	
			blockContext[0].t0							= t0;
			blockContext[0].t1							= t1;
			blockContext[0].tMin						= tMin;
			blockContext[0].tMax						= tMax;
			blockContext[0].tIn							= tIn;
			blockContext[0].tOut						= tOut;
		}

#if HLSL_DEBUG_PIXEL == 1
		uint debugTraceID;
		if ( jobDebugBlock )
		{			
			InterlockedAdd( blockContext[0].debugTraceCount, 1, debugTraceID );
			debugTraces[debugTraceID].blockCenter = block.center;
			debugTraces[debugTraceID].blockOccupancy = block.occupancy;
			debugTraces[debugTraceID].org = rayOrgWS;
			debugTraces[debugTraceID].dir = rayDir;			
			debugTraces[debugTraceID].tIn = tIn;
			debugTraces[debugTraceID].tOut = tOut;
			debugTraces[debugTraceID].hitFoundCoords = (int4)0;
			debugTraces[debugTraceID].hitFailBits = 0;
		}
#endif //#if HLSL_DEBUG_PIXEL == 1

#endif // #if HLSL_DEBUG_BLOCK == 1
		
		if ( tIn > tOut )
			continue;

#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockTraceStats[0].traceProcessedCount, 1 );
#endif // #if BLOCK_GET_STATS == 1

		const float scale = c_blockGridScales[jobMip].z;
		const float offset = HLSL_CELL_SUPER_SAMPLING_WIDTH * 0.5f;	
		const float3 pIn = mad( rayDir, tIn, rayOrgWS );
		const float3 coordIn = (pIn - block.center) * scale + offset;			
		int3 coords = min( (int3)coordIn, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
		float3 tCur = tMin;
		const float3 tDelta = c_blockGridScales[jobMip].w * abs( rayInvDir );		
		for ( uint phase = 0; phase < 2; ++phase )
		{
			const float3 tNext = tCur + tDelta;
			tCur.x = tNext.x < tIn ? tNext.x : tCur.x;
			tCur.y = tNext.y < tIn ? tNext.y : tCur.y;
			tCur.z = tNext.z < tIn ? tNext.z : tCur.z;
		}		

		const int3 step = rayDir < 0.0f ? -1.0f : 1.0f;
		const uint2 jobPixelCoord = uint2( jobPixelOffsetX + jobPixelX, jobPixelOffsetY + jobPixelY );

#if HLSL_DEBUG_BLOCK == 1
		if ( debug )
		{
			blockContext[0].scale						= scale;
			blockContext[0].offset						= offset;	
			blockContext[0].pIn							= pIn;
			blockContext[0].coordIn						= coordIn;			
			blockContext[0].coords						= coords;
			blockContext[0].tCur						= tCur;
			blockContext[0].tDelta						= tDelta;		
			blockContext[0].step						= step;
			blockContext[0].jobPixelCoord				= jobPixelCoord;
		}

#if HLSL_DEBUG_PIXEL == 1
		if ( jobDebugBlock )
		{
			debugTraces[debugTraceID].scale			= scale;
			debugTraces[debugTraceID].offset		= offset;	
			debugTraces[debugTraceID].pIn			= pIn;
			debugTraces[debugTraceID].coordIn		= coordIn;			
			debugTraces[debugTraceID].coords		= coords;
			debugTraces[debugTraceID].tCur			= tCur;
			debugTraces[debugTraceID].tDelta		= tDelta;		
			debugTraces[debugTraceID].step			= step;
			debugTraces[debugTraceID].jobPixelCoord	= jobPixelCoord;
		}
#endif //#if HLSL_DEBUG_PIXEL == 1
		
#endif // #if HLSL_DEBUG_BLOCK == 1

		uint stopTraversal = 0;
		while ( !stopTraversal )
		{
#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockTraceStats[0].traceStepCount, 1 );
#endif // #if BLOCK_GET_STATS == 1
			const uint occupancyBit = coords.z * HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ + coords.y * HLSL_CELL_SUPER_SAMPLING_WIDTH + coords.x;
			if ( (block.occupancy & (1 << occupancyBit)) != 0 )
			{				
				const uint pixelTraceBucket = jobBlockID * traceCellWidthPerBlock + Div3( jobPixelCoord.y );
				const uint pixelTraceCellBit = Div3( jobPixelCoord.x ) * HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ + Mod3( jobPixelCoord.y ) * HLSL_CELL_SUPER_SAMPLING_WIDTH + Mod3( jobPixelCoord.x );
				InterlockedOr( g_traceCellBits[pixelTraceBucket], 1 << pixelTraceCellBit );

#if HLSL_DEBUG_BLOCK == 1
				if ( debug )
					blockContext[0].hitFoundCoords = int4( coords, 1 );

#if HLSL_DEBUG_PIXEL == 1
				if ( jobDebugBlock )
					debugTraces[debugTraceID].hitFoundCoords = int4( coords, 1 );
#endif //#if HLSL_DEBUG_PIXEL == 1

#endif // #if HLSL_DEBUG_BLOCK == 1

				stopTraversal = 2;
			}
			else
			{
#if 1

#if HLSL_DEBUG_BLOCK == 1
				if ( debug )
					blockContext[0].hitFailBits |= 1 << occupancyBit;

#if HLSL_DEBUG_PIXEL == 1
				if ( jobDebugBlock )
					debugTraces[debugTraceID].hitFailBits |= 1 << occupancyBit;
#endif //#if HLSL_DEBUG_PIXEL == 1

#endif // #if HLSL_DEBUG_BLOCK == 1

				const float3 tNext = tCur + tDelta;
				uint nextAxis;
				if ( tNext.x < tNext.y && tNext.x < tNext.z )
				{
					nextAxis = 0;
					tCur.x = tNext.x;
					coords.x += step.x;
				}
				else if ( tNext.y < tNext.z )
				{
					nextAxis = 1;
					tCur.y = tNext.y;
					coords.y += step.y;
				}
				else
				{
					nextAxis = 2;
					tCur.z = tNext.z;
					coords.z += step.z;
				}
		
				if ( coords[nextAxis] < 0 || coords[nextAxis] >= HLSL_CELL_SUPER_SAMPLING_WIDTH )
				{
#if HLSL_DEBUG_BLOCK == 1
#if HLSL_DEBUG_PIXEL == 1
					if ( debug )
						blockContext[0].hitFoundCoords = int4( coords, -1 );
#endif //#if HLSL_DEBUG_PIXEL == 1
#endif // #if HLSL_DEBUG_BLOCK == 1

					stopTraversal = 1;
				}			
#endif
			}

		}
#if BLOCK_GET_STATS == 1
		InterlockedAdd( blockTraceStats[0].traceHitCount, stopTraversal == 2 ? 1 : 0 );
#endif // #if BLOCK_GET_STATS == 1
	}
}

void Output( BlockCell blockCell, float pixelDepth, uint blockID, uint2 minPixelCoords, bool debug )
{
	const uint bucketBase = blockID * traceCellWidthPerBlock;

	for ( uint y = 0; y < 3; ++y )
	{
		if ( g_traceCellBits[bucketBase+y] == 0 )
			continue;

		const uint pixelOccupancyMask = (1 << (HLSL_CELL_SUPER_SAMPLING_WIDTH * 3))-1;
		for ( uint x = 0; x < 3; ++x )
		{
			const uint pixelOccupancy = (g_traceCellBits[bucketBase+y] >> (x * HLSL_CELL_SUPER_SAMPLING_WIDTH * 3)) & pixelOccupancyMask;
			if ( pixelOccupancy == 0 )
				continue;

			uint blockCellItemID;
			InterlockedAdd( blockContext[0].cellItemCount, 1, blockCellItemID );
			blockCellItemID += 1;

			const uint2 pixelCoords = minPixelCoords + uint2( x, y );
			const uint pixelID = mad( pixelCoords.y, c_blockFrameSize.x, pixelCoords.x );

			uint nextBlockCellItemID;
			InterlockedExchange( firstBlockCellItemIDs[pixelID], blockCellItemID, nextBlockCellItemID );
			
			BlockCellItem blockCellItem;
			blockCellItem.r8g8b8a8 = (blockCell.color & ~0xFF) | 0xFF;
			blockCellItem.u8v8w8h8 = pixelOccupancy;
			blockCellItem.depth = pixelDepth;
#if HLSL_DEBUG_BLOCK == 1
			blockCellItem.packedID = blockID;
#endif // #if HLSL_DEBUG_BLOCK == 1
			blockCellItem.nextID = nextBlockCellItemID;

			blockCellItems[blockCellItemID] = blockCellItem;

#if HLSL_DEBUG_BLOCK == 1
			if ( debug )
			{
				blockContext[0].pixelColors[y][x] = blockCellItem.r8g8b8a8;
				blockContext[0].pixelOccupancies[y][x] = blockCellItem.u8v8w8h8;
				blockContext[0].pixelDepths[y][x] = blockCellItem.depth;
			}
#endif // #if HLSL_DEBUG_BLOCK == 1

#if BLOCK_GET_STATS == 1
			InterlockedAdd( blockTraceStats[0].pixelGridCount, 1 );
			InterlockedAdd( blockTraceStats[0].pixelCellCount, countbits( pixelOccupancy ) );
#endif // #if BLOCK_GET_STATS == 1
		}
	}
}

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
	const uint packedID = DTid.x;
	const uint blockID = GTid.x;	

	bool debug = false;
#if HLSL_DEBUG_BLOCK == 1
	if ( GRID_CELL_BUCKET == 3 && packedID == c_blockDebugPackedID )
		InterlockedMax( g_debugBlockID, (int)blockID );
	else
		InterlockedMin( g_debugBlockID, -1 );
	debug = GRID_CELL_BUCKET == 3 && packedID == c_blockDebugPackedID;
#endif	
	
	// Decode

	const uint maxCellCount = trace_culledBlockCount * GRID_CELL_COUNT;

	BlockCell blockCell = (BlockCell)0;
	bool cull = packedID >= maxCellCount || !PackedColor_Unpack( packedID, 0, blockCell );

#if BLOCK_GET_STATS == 1
	InterlockedAdd( blockTraceStats[0].blockInputCount, 1 );	
#endif // #if BLOCK_GET_STATS == 1

	// Parallel Init

	if ( !cull )
		ParallelInit( blockID );

	AllMemoryBarrierWithGroupSync();

	// Clip
		
	uint2 multiSampledMinPixelCoords = (uint2)0;
	uint2 multiSampledMaxPixelCoords = (uint2)0;
	float pixelDepth = 1e32f;

	bool debugBlock = false;
	cull = cull || Clip( blockCell, multiSampledMinPixelCoords, multiSampledMaxPixelCoords, pixelDepth, packedID, debug, debugBlock );

#if HLSL_DEBUG_BLOCK == 1
	if ( debug )
	{
		blockContext[0].packedID = packedID;
		blockContext[0].blockID = blockID;
		blockContext[0].cull = cull;
	}

#if HLSL_DEBUG_PIXEL == 1
	if ( !cull && debugBlock )
	{
		uint debugBlockID;
		InterlockedAdd( blockContext[0].debugBlockCount, 1, debugBlockID );
		debugBlocks[debugBlockID].posWS = blockCell.posWS;
		debugBlocks[debugBlockID].color = blockCell.color;
		debugBlocks[debugBlockID].mip = blockCell.mip;
		debugBlocks[debugBlockID].occupancy = blockCell.occupancy;
		const uint2 multiSampledSize = 1 + multiSampledMaxPixelCoords - multiSampledMinPixelCoords;
		const uint multiSampledPixelCount = multiSampledSize.x * multiSampledSize.y;
		debugBlocks[debugBlockID].jobCount = multiSampledPixelCount;
	}
#endif // #if HLSL_DEBUG_PIXEL == 1

#endif // #if HLSL_DEBUG_BLOCK == 1
	
	// Assignment

	uint2 minPixelCoords = (uint2)0;
	if ( !cull )
		Assign( blockCell, blockID, multiSampledMinPixelCoords, multiSampledMaxPixelCoords, minPixelCoords, debug, debugBlock );

	// Parallel Trace

	AllMemoryBarrierWithGroupSync();

	if ( !c_blockSkipTrace )
		ParallelTrace( blockID );

	AllMemoryBarrierWithGroupSync();

	// Output

	if ( !cull )
		Output( blockCell, pixelDepth, blockID, minPixelCoords, debug );	
}
