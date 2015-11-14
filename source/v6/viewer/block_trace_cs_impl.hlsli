#define HLSL

#include "common_shared.h"
#include "block_cell.hlsli"

#define BUFFER_WIDTH HLSL_PIXEL_SUPER_SAMPLING_WIDTH

RWStructuredBuffer< BlockCellItem > blockCellItems			: register( HLSL_BLOCK_CELL_ITEM_UAV );
RWBuffer< uint > firstBlockCellItemIDs						: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV );
RWStructuredBuffer< BlockContext > blockContext				: register( HLSL_BLOCK_CONTEXT_UAV );

struct TraceBlock_s
{
	float3	center;
	uint	occupancy;
	uint	multiSampledMinPixel_basex14y14_offsetx2y2;
	uint	jobWidth16_jobOffset12_mip4;
};

#if HLSL_PIXEL_SUPER_SAMPLING_WIDTH == 3
static const uint			pixelCountMax = 49;
static const uint			traceCellWidthPerBlock = 3;
static const uint			traceJobPerInt = 4;
#endif

groupshared TraceBlock_s	g_blocks[HLSL_TRACE_THREAD_GROUP_SIZE];
groupshared uint			g_traceCellBits[HLSL_TRACE_THREAD_GROUP_SIZE * traceCellWidthPerBlock];
groupshared uint			g_jobs[(HLSL_TRACE_THREAD_GROUP_SIZE * pixelCountMax + traceJobPerInt - 1) / traceJobPerInt];
groupshared uint			g_jobCount = 0;

void ParallelInit( uint blockID )
{
	g_jobCount == 0;

	const uint bucketBase = blockID * traceCellWidthPerBlock;
	for ( uint bucketOffset = 0; bucketOffset < traceCellWidthPerBlock; ++bucketOffset )
		g_traceCellBits[bucketBase + bucketOffset] = 0;	
}

bool Clip( BlockCell blockCell, out uint2 multiSampledMinPixelCoords, out uint2 multiSampledMaxPixelCoords, out float pixelDepth, bool debug )
{
	const matrix worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );
		
	uint3 boxMin = uint3( 2, 2, 2 );
	uint3 boxMax = uint3( 0, 0, 0 );
	
	{
		uint occupancyBit = 0;
		for ( uint z = 0; z < HLSL_PIXEL_SUPER_SAMPLING_WIDTH; ++z )
		{
			for ( uint y = 0; y < HLSL_PIXEL_SUPER_SAMPLING_WIDTH; ++y )
			{
				for ( uint x = 0; x < HLSL_PIXEL_SUPER_SAMPLING_WIDTH; ++x, ++occupancyBit )
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
	for ( uint vertexID = 0; vertexID < 8; ++vertexID )
	{
		const float3 posWS = pointMin + delta * vertices[vertexID];
		const float4 posCS = mul( worldToProjMatrix, float4( posWS, 1.0f ) );
		const float2 screenPos = posCS.xy * rcp( posCS.w );		
		minScreenPos = min( minScreenPos, screenPos );
		maxScreenPos = max( maxScreenPos, screenPos );
		pixelDepth = min( pixelDepth, posCS.w );
		clippedCount += any( abs( posCS.xyz ) > posCS.w ) ? 1 : 0;
	}
	
	const float2 screenPos = (minScreenPos + maxScreenPos) * 0.5f;
	const float2 screenRadius = (maxScreenPos - minScreenPos) * 0.5f;
	const float2 multiSampledPixelPos = mad( screenPos, 0.5f, 0.5f ) * c_blockMultiSampledFrameSize;
	const float2 multiSampledPixelRadius = min( screenRadius * 0.5f * c_blockMultiSampledFrameSize, HLSL_PIXEL_SUPER_SAMPLING_WIDTH );
	multiSampledMinPixelCoords = clamp( int2( multiSampledPixelPos - multiSampledPixelRadius ), 0, c_blockMultiSampledFrameSize-1 );
	multiSampledMaxPixelCoords = clamp( int2( multiSampledPixelPos + multiSampledPixelRadius ), 0, c_blockMultiSampledFrameSize-1 );	

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
#endif // #if HLSL_DEBUG_BLOCK == 1

	return clippedCount == 8;
}

void Assign( BlockCell blockCell, uint blockID, uint2 multiSampledMinPixelCoords, uint2 multiSampledMaxPixelCoords, out uint2 minPixelCoords, bool debug )
{
	minPixelCoords = uint2( (multiSampledMinPixelCoords + 0.5f) / float( HLSL_PIXEL_SUPER_SAMPLING_WIDTH ) );
	const uint2 multiSampledMinPixelBase = minPixelCoords * HLSL_PIXEL_SUPER_SAMPLING_WIDTH;
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
	block.multiSampledMinPixel_basex14y14_offsetx2y2 = (multiSampledMinPixelBase.x << 18) | (multiSampledMinPixelBase.y << 4) | (multiSampledMinPixelOffset.y << 2) | multiSampledMinPixelOffset.x;
	block.jobWidth16_jobOffset12_mip4 = (multiSampledSize.x << 16) | (jobOffset << 4) | blockCell.mip;
	g_blocks[blockID] = block;
	
	for ( uint jobRank = 0; jobRank < multiSampledPixelCount; ++jobRank )
	{
		const uint jobID = jobOffset + jobRank;
		g_jobs[jobID >> 2] = blockID << ((jobID&3) * 8);
	}
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

void ParallelTrace( uint blockID, bool debug  )
{
	const float3 rayOrgWS = float3( c_blockViewToObject[0].w, c_blockViewToObject[1].w, c_blockViewToObject[2].w );

	for ( uint jobID = blockID; jobID < g_jobCount; jobID += HLSL_TRACE_THREAD_GROUP_SIZE )
	{
		const uint jobBlockID = (g_jobs[jobID >> 2] >> ((jobID&3) * 8)) & 0xFF;
		const TraceBlock_s block = g_blocks[jobBlockID];
		const uint jobWidth = block.jobWidth16_jobOffset12_mip4 >> 16;
		const uint jobOffset = (block.jobWidth16_jobOffset12_mip4 >> 4) & 0xFFF;
		const uint jobMip = block.jobWidth16_jobOffset12_mip4 & 0xF;
		const uint jobPixelID = jobID - jobOffset;
		const uint jobPixelBaseX = block.multiSampledMinPixel_basex14y14_offsetx2y2 >> 18;
		const uint jobPixelBaseY = (block.multiSampledMinPixel_basex14y14_offsetx2y2 >> 4) & 0x3FFF;
		const uint jobPixelOffsetX = (block.multiSampledMinPixel_basex14y14_offsetx2y2 >> 2) & 3;
		const uint jobPixelOffsetY = block.multiSampledMinPixel_basex14y14_offsetx2y2 & 3;
		const float lineCount = trunc( (jobPixelID + 0.5f) * rcp( jobWidth ) );
		const float jobPixelX = jobPixelID - mad( lineCount,  jobWidth, -0.5f );
		const float jobPixelY = lineCount + 0.5f;		
		
		const float2 jobMultiSampledScreenPos = float2( jobPixelBaseX + jobPixelOffsetX + jobPixelX, jobPixelBaseY + jobPixelOffsetY + jobPixelY );
		const float3 rayEndVS = float3( mad( jobMultiSampledScreenPos, c_blockScreenToClipScale, c_blockScreenToClipOffset ), -c_blockZNear );
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
		if ( debug && jobID == blockID )
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
#endif // #if HLSL_DEBUG_BLOCK == 1
		
		if ( tIn > tOut )
			continue;

		const float scale = c_blockGridScales[jobMip].z;
		const float offset = HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 0.5f;	
		const float3 pIn = mad( rayDir, tIn, rayOrgWS );
		const float3 coordIn = (pIn - block.center) * scale + offset;			
		int3 coords = min( (int3)coordIn, HLSL_PIXEL_SUPER_SAMPLING_WIDTH-1 );
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
		if ( debug && jobID == blockID )
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
#endif // #if HLSL_DEBUG_BLOCK == 1

#if 1
		for ( ;; )
		{
			const uint occupancyBit = coords.z * HLSL_PIXEL_SUPER_SAMPLING_WIDTH_SQ + coords.y * HLSL_PIXEL_SUPER_SAMPLING_WIDTH + coords.x;
			if ( (block.occupancy & (1 << occupancyBit)) != 0 )
			{				
				const uint pixelBucket = jobBlockID * traceCellWidthPerBlock + Div3( jobPixelCoord.y );
				g_traceCellBits[pixelBucket] |= 1 << (Div3( jobPixelCoord.x ) * HLSL_PIXEL_SUPER_SAMPLING_WIDTH_SQ + Mod3( jobPixelCoord.y ) * HLSL_PIXEL_SUPER_SAMPLING_WIDTH + Mod3( jobPixelCoord.x  ));
				
				break;
			}

			const float3 tNext = tCur + tDelta;
			uint nextAxis;
			if ( tNext.x < tNext.y && tNext.x < tNext.z )
				nextAxis = 0;
			else if ( tNext.y < tNext.z )
				nextAxis = 1;
			else
				nextAxis = 2;
			const int3 axes[] = { uint3( 1, 0, 0 ), uint3( 0, 1, 0 ), uint3( 0, 0, 1 ) };

			tCur = lerp( tCur, tNext, axes[nextAxis] );
			coords += step * axes[nextAxis];
		
			if ( coords[nextAxis] < 0 || coords[nextAxis] >= HLSL_PIXEL_SUPER_SAMPLING_WIDTH )
				break;
		}
#endif
	}
}

void Output( BlockCell blockCell, float pixelDepth, uint blockID, uint2 minPixelCoords )
{
	const uint bucketBase = blockID * traceCellWidthPerBlock;

	for ( uint y = 0; y < 3; ++y )
	{
		if ( g_traceCellBits[bucketBase+y] == 0 )
			continue;

		const uint pixelOccupancyMask = (1 << (HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 3))-1;
		for ( uint x = 0; x < 3; ++x )
		{
			const uint pixelOccupancy = (g_traceCellBits[bucketBase+y] >> (x * HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 3)) & pixelOccupancyMask;
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
			blockCellItem.nextID = nextBlockCellItemID;

			blockCellItems[blockCellItemID] = blockCellItem;						
		}
	}
}

[ numthreads( HLSL_TRACE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
#if HLSL_DEBUG_BLOCK == 1
	const bool debug = GRID_CELL_BUCKET == 2 && DTid.x == 1;
#endif	
	
	const uint packedID = DTid.x;
	const uint blockID = GTid.x;

	// Decode

	const uint maxCellCount = blockIndirectArgs[block_count_offset( GRID_CELL_BUCKET )] * GRID_CELL_COUNT;

	BlockCell blockCell = (BlockCell)0;
	bool cull = packedID >= maxCellCount || !PackedColor_Unpack( packedID, blockCell );

	// Parallel Init

	ParallelInit( blockID );

	AllMemoryBarrierWithGroupSync();

	// Clip
		
	uint2 multiSampledMinPixelCoords = (uint2)0;
	uint2 multiSampledMaxPixelCoords = (uint2)0;
	float pixelDepth = 1e32f;
	cull = cull || Clip( blockCell, multiSampledMinPixelCoords, multiSampledMaxPixelCoords, pixelDepth, debug );

#if HLSL_DEBUG_BLOCK == 1
	if ( debug )
		blockContext[0].cull = cull;
#endif // #if HLSL_DEBUG_BLOCK == 1
	
	// Assignment

	uint2 minPixelCoords = (uint2)0;
	if ( !cull )
		Assign( blockCell, blockID, multiSampledMinPixelCoords, multiSampledMaxPixelCoords, minPixelCoords, debug );		

	// Parallel Trace

	AllMemoryBarrierWithGroupSync();

	ParallelTrace( blockID, debug );

	AllMemoryBarrierWithGroupSync();

	// Output

	Output( blockCell, pixelDepth, blockID, minPixelCoords );	
}
