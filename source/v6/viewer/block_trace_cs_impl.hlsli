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
	uint	multiSampledMinPixelBasex16y16;
	uint	jobWidth6_jobOffset12_mip4;
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

[ numthreads( HLSL_TRACE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
	const uint packedID = DTid.x;
	const uint blockID = GTid.x;
	
	const uint maxCellCount = blockIndirectArgs[block_count_offset( GRID_CELL_BUCKET )] * GRID_CELL_COUNT;

	bool cull = packedID >= maxCellCount;

	// Decode

	BlockCell blockCell = (BlockCell)0;
	cull = cull || !PackedColor_Unpack( packedID, blockCell );

	// Clip

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
	float pixelDepth = 1e32f;
	
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

	cull = cull || clippedCount == 8;

	const float2 screenPos = (minScreenPos + maxScreenPos) * 0.5f;
	const float2 screenRadius = (maxScreenPos - minScreenPos) * 0.5f;
	const float2 multiSampledPixelPos = mad( screenPos, 0.5f, 0.5f ) * c_blockMultiSampledFrameSize;
	const float2 multiSampledPixelRadius = min( screenRadius * 0.5f * c_blockMultiSampledFrameSize, HLSL_PIXEL_SUPER_SAMPLING_WIDTH );
	const uint2 multiSampledMinPixelCoords = clamp( int2( multiSampledPixelPos - multiSampledPixelRadius ), 0, c_blockMultiSampledFrameSize-1 );
	const uint2 multiSampledMaxPixelCoords = clamp( int2( multiSampledPixelPos + multiSampledPixelRadius ), 0, c_blockMultiSampledFrameSize-1 );
	const uint2 minPixelCoords = uint2( (multiSampledMinPixelCoords + 0.5f) / float( HLSL_PIXEL_SUPER_SAMPLING_WIDTH ) );
	const uint2 multiSampledMinPixelBase = minPixelCoords * HLSL_PIXEL_SUPER_SAMPLING_WIDTH;	
	const uint2 multiSampledMinPixelOffset = multiSampledMinPixelCoords - multiSampledMinPixelBase;

	// Affectation
		
	const uint2 multiSampledSize = 1 + multiSampledMaxPixelCoords - multiSampledMinPixelCoords;
	const uint multiSampledPixelCount = multiSampledSize.x * multiSampledSize.y;
	uint jobOffset;
	InterlockedAdd( g_jobCount, multiSampledPixelCount, jobOffset );

	TraceBlock_s block;
	block.center = blockCell.posWS;
	block.multiSampledMinPixelBasex16y16 = (multiSampledMinPixelBase.x << 16) || (multiSampledMinPixelBase.y << 16);
	block.jobWidth6_jobOffset12_mip4 = (multiSampledSize.x << 16) | (jobOffset << 4) | blockCell.mip;
	g_blocks[blockID] = block;
	
	if ( !cull )
	{
		for ( uint jobRank = 0; jobRank < multiSampledPixelCount; ++jobRank )
		{
			const uint jobID = jobOffset + jobRank;
			g_jobs[jobID >> 2] = blockID << ((jobID&3) * 8);
		}
	}

	AllMemoryBarrierWithGroupSync();

	// Trace

	const float3 rayOrg = float3( c_blockViewToObject[0].w, c_blockViewToObject[1].w, c_blockViewToObject[2].w );

	for ( uint jobID = 0; jobID < g_jobCount; jobID += HLSL_TRACE_THREAD_GROUP_SIZE )
	{
		const uint jobBlockID = g_jobs[jobID >> 2] >> ((jobID&3) * 8);		
		const TraceBlock_s block = g_blocks[jobBlockID];
		const uint jobWidth = (block.jobWidth6_jobOffset12_mip4 >> 16) & 0x3F;
		const uint jobOffset = (block.jobWidth6_jobOffset12_mip4 >> 4) & 0xFFF;
		const uint jobMip = block.jobWidth6_jobOffset12_mip4 & 0xF;
		const uint jobPixelID = jobID - jobOffset;
		const float jobPixelBaseX = block.multiSampledMinPixelBasex16y16 >> 16;
		const float jobPixelBaseY = block.multiSampledMinPixelBasex16y16 & 0xFFFF;
		const float jobPixelOffsetX = fmod( jobPixelID, jobWidth ) + 0.5f;
		const float jobPixelOffsetY = mad( jobPixelID, rcp( jobWidth ), 0.5f );
		
		const float2 jobMultiSampledScreenPos = float2( jobPixelBaseX + jobPixelOffsetX, jobPixelBaseY + jobPixelOffsetY );
		const float3 rayEndVS = float3( mad( jobMultiSampledScreenPos, c_blockScreenToClipScale, c_blockScreenToClipOffset ), -c_blockZNear );
		const float3 rayEndWS = mul( c_blockViewToObject, float4( rayEndVS, 1.0f ) ).xyz;
		const float3 rayDir = normalize( rayEndWS - rayOrg );
		const float3 rayInvDir = rcp( rayDir );
		const float3 alpha = (block.center - rayOrg) * rayInvDir;
		const float3 beta = c_blockGridScales[jobMip].x * rayInvDir;	
		const float3 t0 = alpha + beta;
		const float3 t1 = alpha - beta;
		const float3 tMin = min( t0, t1 );
		const float3 tMax = max( t0, t1 );
		const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
		const float tOut = min( min( tMax.x, tMax.y ), tMax.z );
		
		if ( tIn > tOut )
			continue;

		const float scale = c_blockGridScales[jobMip].y;
		const float offset = HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 0.5f;
	
		const float3 tDelta = c_blockGridScales[jobMip].z * abs( rayInvDir );	

		const float3 pIn = mad( rayDir, tIn, rayOrg );
		const float3 coordIn = (pIn - block.center) * scale + offset;
		
		const int x = min( (int)coordIn.x, HLSL_PIXEL_SUPER_SAMPLING_WIDTH-1 );
		const int y = min( (int)coordIn.y, HLSL_PIXEL_SUPER_SAMPLING_WIDTH-1 );
		const int z = min( (int)coordIn.z, HLSL_PIXEL_SUPER_SAMPLING_WIDTH-1 );
		const int3 step = sign( rayDir );
		float3 tCur = tMin;
		int3 coords = int3( x, y, z );
		for ( uint phase = 0; phase < 2; ++phase )
		{
			const float3 tNext = tCur + tDelta;
			tCur.x = tNext.x < tIn ? tNext.x : tCur.x;
			tCur.y = tNext.y < tIn ? tNext.y : tCur.y;
			tCur.z = tNext.z < tIn ? tNext.z : tCur.z;
		}

		for ( ;; )
		{
			const uint pixelBucket = jobBlockID * traceCellWidthPerBlock + (jobPixelID >> 5);
			g_traceCellBits[pixelBucket] |= 1 << (jobPixelID & 0x1F);

			const float3 tNext = tCur + tDelta;
			uint nextAxis;
			if ( tNext.x < tNext.y )
				nextAxis = tNext.x < tNext.z ? 0 : 2;
			else
				nextAxis = tNext.y < tNext.z ? 1 : 2;
			const int3 axes[] = { uint3( 1, 0, 0 ), uint3( 0, 1, 0 ), uint3( 0, 0, 1 ) };

			tCur = lerp( tCur, tNext, axes[nextAxis] );
			coords += step * axes[nextAxis];
		
			if ( coords[nextAxis] < 0 || coords[nextAxis] >= HLSL_PIXEL_SUPER_SAMPLING_WIDTH )
				break;
		}
	}

	AllMemoryBarrierWithGroupSync();

	// Output
	
	const uint pixelBucketBase = blockID * traceCellWidthPerBlock;
	for ( uint y = 0; y < 3; ++y )
	{
		if ( g_traceCellBits[pixelBucketBase+y] == 0 )
			continue;

		const uint pixelOccupancyMask = (1 << (HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 3))-1;
		for ( uint x = 0; x < 3; ++x )
		{
			const uint pixelOccupancy = (g_traceCellBits[pixelBucketBase+y] >> (x * HLSL_PIXEL_SUPER_SAMPLING_WIDTH * 3)) & pixelOccupancyMask;
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
