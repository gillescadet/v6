#define HLSL

#include "trace_shared.h"

Buffer< uint > blockPatchCounters						: REGISTER_SRV( HLSL_BLOCK_PATCH_COUNTERS_SLOT );
StructuredBuffer< BlockPatch > blockPatches				: REGISTER_SRV( HLSL_BLOCK_PATCHES_SLOT );
StructuredBuffer< uint64 > blockCellPresences			: REGISTER_SRV( HLSL_BLOCK_CELL_PRESENCE_SLOT );
Buffer< uint > blockCellEndColors						: REGISTER_SRV( HLSL_BLOCK_CELL_END_COLOR_SLOT );
StructuredBuffer< uint64 > blockCellColorIndices0		: REGISTER_SRV( HLSL_BLOCK_CELL_COLOR_INDEX0_SLOT );
StructuredBuffer< uint64 > blockCellColorIndices1		: REGISTER_SRV( HLSL_BLOCK_CELL_COLOR_INDEX1_SLOT );

RWTexture2D< float4 > outputColors						: REGISTER_UAV( HLSL_COLOR_SLOT );
RWBuffer< uint > outputDisplacements					: REGISTER_UAV( HLSL_DISPLACEMENT_SLOT );
#if BLOCK_DEBUG == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats	: REGISTER_UAV( HLSL_TRACE_STATS_SLOT );
#endif // #if BLOCK_DEBUG == 1

struct TracePatch
{
	uint	blockPosID;
	uint	none16_xMask8_yMask8; // optim: separate this and merge two patches
	uint3	cellMinRange;
	uint3	cellExtent;
	float3	boxMinRS;
	float3	boxMaxRS;
	float	cellSize;
	float	invCellSize;
	uint64	blockCellPresence;
	uint	blockColorPalette[4];
	uint	xdsp16_ydsp16;
};

struct Hit
{
	uint	blockPosID;
	uint	blockColorPalette[4];
	uint	cellRank;
	float	depth;
	uint	xdsp16_ydsp16;
};

groupshared TracePatch		gs_patches[64];
groupshared uint			gs_patchCount;
groupshared uint			gs_pageCount;

uint BuildBitRangeMask( uint offset, uint width )
{
	return ((1 << width) - 1) << offset;
}

bool Assert( bool condition, uint id )
{
#if BLOCK_DEBUG == 1
	if ( !condition )
	{
		uint prev;
		InterlockedOr( blockTraceStats[0].assertFailedBits, 1 << id, prev );
		return prev == 0;
	}
	else
#endif // #if BLOCK_DEBUG == 1
	{
		return false;
	}
}

uint HashU32( uint seed )
{
	// http://www.reedbeta.com/blog/2013/01/12/quick-and-easy-gpu-random-numbers-in-d3d11/

	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	return seed;
}

float3 U32ToColor( uint v )
{
	const uint colorHash = HashU32( v );
	return float3( (colorHash >> 0) & 0xFF, (colorHash >> 8) & 0xFF, (colorHash >> 16) & 0xFF ) * (1.0f / 255.0f);
}

TracePatch LoadPatch( uint patchID )
{
	const BlockPatch blockPatch = blockPatches[patchID];

	// Compute bounding box

	const uint packedBlockPos = blockPatch.packedBlockPos;
	const uint mip = packedBlockPos >> 28;
	const uint blockPos = packedBlockPos & 0x0FFFFFFF;
	const uint gridMacroMask = (1 << c_traceGridMacroShift)-1;
	const uint blockPosX = (blockPos >> (c_traceGridMacroShift*0)) & gridMacroMask;
	const uint blockPosY = (blockPos >> (c_traceGridMacroShift*1)) & gridMacroMask;
	const uint blockPosZ = (blockPos >> (c_traceGridMacroShift*2)) & gridMacroMask;

	uint3 cellMinRange;
	cellMinRange.x = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 26) & 3;
	cellMinRange.y = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 24) & 3;
	cellMinRange.z = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 22) & 3;
			
	uint3 cellMaxRange;
	cellMaxRange.x = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 20) & 3;
	cellMaxRange.y = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 18) & 3;
	cellMaxRange.z = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 16) & 3;

	uint3 cellExtent = cellMaxRange - cellMinRange;

	const float gridScale = c_traceGridScales[mip].x;
	const float cellSize = c_traceGridScales[mip].y;
	const float invCellSize = c_traceGridScales[mip].z;
	const uint3 cellCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
	const uint3 cellMinCoords = cellCoords + cellMinRange;
	const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_traceGridCenters[mip].xyz;

	const float3 boxMinRS = posMinWS - c_traceRayOrg.xyz; 
	const float3 boxMaxRS = mad( float3( 1 + cellExtent ), cellSize, boxMinRS );

	// Build xy mask

	const uint w = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 4) & 0xF;
	const uint h = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 0) & 0xF;

#if BLOCK_DEBUG == 1
	InterlockedAdd( blockTraceStats[0].pixelTraceCount, w * h );
#endif // #if BLOCK_DEBUG == 1
		
	Assert( w > 0 && w <= 8, 0 );
	Assert( h > 0 && h <= 8, 1 );
	
	const uint x = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 12) & 0xF;
	const uint y = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >>  8) & 0xF;
	
	Assert( x + w <= 8, 2 );
	Assert( y + h <= 8, 3 );
	
	const uint xMask = BuildBitRangeMask( x, w );
	const uint yMask = BuildBitRangeMask( y, h );
	
	Assert( firstbitlow( xMask ) == x, 4 );
	Assert( firstbitlow( yMask ) == y, 5 );
	Assert( countbits( xMask ) == w, 6 );
	Assert( countbits( yMask ) == h, 7 );
	Assert( firstbithigh( xMask ) == (x + w - 1), 8 );
	Assert( firstbithigh( yMask ) == (y + h - 1), 9 );

	// Decode end colors

	const uint cellEndColors = blockCellEndColors[blockPatch.blockPosID];
	const uint color0 = (cellEndColors >>  0) & 0xFFFF;
	const uint color1 = (cellEndColors >> 16) & 0xFFFF;

	const uint maxColorR = ((color0 >> 11) & 0x1F) << 3;
	const uint maxColorG = ((color0 >>  5) & 0x3F) << 2;
	const uint maxColorB = ((color0 >>  0) & 0x1F) << 3;

	const uint minColorR = ((color1 >> 11) & 0x1F) << 3;
	const uint minColorG = ((color1 >>  5) & 0x3F) << 2;
	const uint minColorB = ((color1 >>  0) & 0x1F) << 3;

	// Make color palette

	uint colorPalette[4];

	const uint rMax = maxColorR | (maxColorR >> 5);
	const uint gMax = maxColorG | (maxColorG >> 6);
	const uint bMax = maxColorB | (maxColorB >> 5);
	colorPalette[0] = (rMax << 24) | (gMax << 16) | (bMax << 8);

	const uint rMin = minColorR | (minColorR >> 5);
	const uint gMin = minColorG | (minColorG >> 6);
	const uint bMin = minColorB | (minColorB >> 5);
	colorPalette[1] = (rMin << 24) | (gMin << 16) | (bMin << 8);

	const uint r2 = (170 * rMax + 85 * rMin) >> 8;
	const uint g2 = (170 * gMax + 85 * gMin) >> 8;
	const uint b2 = (170 * bMax + 85 * bMin) >> 8;
	colorPalette[2] = (r2 << 24) | (g2 << 16) | (b2 << 8);

	const uint r3 = (85 * rMax + 170 * rMin) >> 8;
	const uint g3 = (85 * gMax + 170 * gMin) >> 8;
	const uint b3 = (85 * bMax + 170 * bMin) >> 8;
	colorPalette[3] = (r3 << 24) | (g3 << 16) | (b3 << 8);

	// Init trace patch
	
	TracePatch tracePatch;
	tracePatch.blockPosID = blockPatch.blockPosID;
	tracePatch.none16_xMask8_yMask8 = (xMask << 8) | yMask;
	tracePatch.cellMinRange = cellMinRange;
	tracePatch.cellExtent = cellExtent;
	tracePatch.boxMinRS = boxMinRS;
	tracePatch.boxMaxRS = boxMaxRS;
	tracePatch.cellSize = cellSize;
	tracePatch.invCellSize = invCellSize;
	tracePatch.blockCellPresence = blockCellPresences[blockPatch.blockPosID];
	tracePatch.blockColorPalette = colorPalette;
	tracePatch.xdsp16_ydsp16 = blockPatch.xdsp16_ydsp16;

	return tracePatch;
}

int Trace( inout Hit hit, float3 rayDir, float3 rayInvDir, uint patchRank )
{
	const TracePatch patch = gs_patches[patchRank];

	const float3 t0 = patch.boxMinRS * rayInvDir;
	const float3 t1 = patch.boxMaxRS * rayInvDir;
	const float3 tMin = min( t0, t1 );
	const float3 tMax = max( t0, t1 );
	const float tIn = max( max( tMin.x, tMin.y ), tMin.z );
	const float tOut = min( min( tMax.x, tMax.y ), tMax.z );
	const float tValid = min( tOut, hit.depth );

	int hitCellID = HLSL_TRACE_STATE_MISS_BLOCK;
	
	if ( tIn < tValid )
	{
		const float3 coordIn = mad( rayDir, tIn, -patch.boxMinRS ) * patch.invCellSize;
		const float3 coordClamped = min( trunc( coordIn ), float3( patch.cellExtent ) );
		uint3 relativeCoords = uint3( coordClamped );

		const float3 tDelta = abs( rayInvDir ) * patch.cellSize;

		const float3 tOffsetPos = coordClamped * tDelta;
		const float3 tOffsetNeg = mad( tDelta, float3( patch.cellExtent ), -tOffsetPos );

		float3 tCur = tMin;
		tCur.x += rayDir.x < 0.0f ? tOffsetNeg.x : tOffsetPos.x;
		tCur.y += rayDir.y < 0.0f ? tOffsetNeg.y : tOffsetPos.y;
		tCur.z += rayDir.z < 0.0f ? tOffsetNeg.z : tOffsetPos.z;

		int3 coordsStep;
		coordsStep.x = rayDir.x < 0.0f ? -1 : 1;
		coordsStep.y = rayDir.y < 0.0f ? -1 : 1;
		coordsStep.z = rayDir.z < 0.0f ? -1 : 1;

		hitCellID = HLSL_TRACE_STATE_MISS_CELL;

		do
		{
			const uint3 cellCoords = relativeCoords + patch.cellMinRange;
			const uint curCellID = mad( cellCoords.z, 16, mad( cellCoords.y, 4, cellCoords.x ) );

			const uint isCellHigh = curCellID >> 5;
			const uint isPresenceLow = (patch.blockCellPresence.low >> curCellID) & 1;
			const uint isPresenceHigh = (patch.blockCellPresence.high >> (curCellID & 31)) & 1;
			if ( (isPresenceLow & ~isCellHigh) | (isPresenceHigh & isCellHigh) )
			{
				hitCellID = curCellID;
				break;
			}

			const float3 tNext = tCur + tDelta;

			const bool nextAxisIsX = (tNext.x < tNext.y) & (tNext.x < tNext.z);
			const bool nextAxisIsYIfNotX = tNext.y < tNext.z;
			const bool nextAxisIsY = !nextAxisIsX & nextAxisIsYIfNotX;
			const bool nextAxisIsZ = !nextAxisIsX & !nextAxisIsYIfNotX;

			tCur.x = nextAxisIsX ? tNext.x : tCur.x;
			tCur.y = nextAxisIsY ? tNext.y : tCur.y;
			tCur.z = nextAxisIsZ ? tNext.z : tCur.z;

			relativeCoords.x += nextAxisIsX ? coordsStep.x : 0;
			relativeCoords.y += nextAxisIsY ? coordsStep.y : 0;
			relativeCoords.z += nextAxisIsZ ? coordsStep.z : 0;

		} while ( (relativeCoords.x <= patch.cellExtent.x ) & (relativeCoords.y <= patch.cellExtent.y ) & (relativeCoords.z <= patch.cellExtent.z) );
	}

	if ( hitCellID >= HLSL_TRACE_STATE_HIT )
	{
		const uint cellMask = (1u << (hitCellID & 31)) - 1;
		const uint cellLowRank = countbits( patch.blockCellPresence.low & cellMask );
		const uint cellHighRank = countbits( patch.blockCellPresence.low ) + countbits( patch.blockCellPresence.high & cellMask );

		hit.blockPosID = patch.blockPosID;
		hit.blockColorPalette = patch.blockColorPalette;
		hit.cellRank = hitCellID < 32 ? cellLowRank : cellHighRank;
		hit.depth = tIn;
		hit.xdsp16_ydsp16 = patch.xdsp16_ydsp16;
	}

	return hitCellID;
}

uint BuildTraceMask( uint group_xMask8_yMask8, uint patchOffset )
{
	uint patchMask = 0;
	for ( uint patchBit = 0; patchBit < 32; ++patchBit )
		patchMask |= ((group_xMask8_yMask8 & gs_patches[patchOffset + patchBit].none16_xMask8_yMask8) == group_xMask8_yMask8) << patchBit;

#if BLOCK_DEBUG == 1
	InterlockedAdd( blockTraceStats[0].pixelEmptyMaskCount, patchMask == 0 );
	InterlockedAdd( blockTraceStats[0].pixelNotEmptyMaskCount, patchMask != 0 );
#endif // #if BLOCK_DEBUG == 1

	return patchMask;
}

void ExecuteTraceMask( inout Hit hit, inout uint patchMask, float3 rayDir, float3 rayInvDir, uint patchOffset )
{
	const uint patchBit = firstbitlow( patchMask );
	const uint patchRank = patchOffset + patchBit;

	const int hitCellID = Trace( hit, rayDir, rayInvDir, patchRank );

	patchMask -= 1u << patchBit;

#if BLOCK_DEBUG == 1
	const uint hitState = max( 0, -hitCellID );
	InterlockedAdd( blockTraceStats[0].pixelHitCounts[hitState], 1 );
	InterlockedAdd( blockTraceStats[0].pixelDoneCount, 1 );
#endif // #if BLOCK_DEBUG == 1
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID )
{
	const uint group = mad( GTid.y, 8, GTid.x );
	const uint tileOffset = mad( Gid.y, c_traceFrameTileSize.x, Gid.x );
	const uint group_xMask8_yMask8 = (1 << (GTid.x + 8)) | (1 << GTid.y);

	if ( group == 0 )
	{
		gs_patchCount = min( blockPatchCounters[tileOffset], HLSL_BLOCK_PATCH_MAX_COUNT_PER_TILE );
		gs_pageCount = (gs_patchCount + 63) >> 6;

#if BLOCK_DEBUG == 1
		InterlockedAdd( blockTraceStats[0].tileInputCounts[max( 1, gs_pageCount ) - 1], 1 );
		InterlockedAdd( blockTraceStats[0].patchInputCount, gs_patchCount );
#endif // #if BLOCK_DEBUG == 1
	}

#if BLOCK_DEBUG == 1
	InterlockedAdd( blockTraceStats[0].pixelInputCount, 1 );
#endif // #if BLOCK_DEBUG == 1

	GroupMemoryBarrierWithGroupSync();

	uint remainingPatchCount = gs_patchCount;
	const uint pageSize = (c_traceFrameTileSize.x * c_traceFrameTileSize.y) * 64;
	const uint pageOffset = mad( tileOffset, 64, group );
	
	// Use one ray per thread

	const float2 jitteredCoords = float2( DTid.xy ) + c_traceJitter;
	const float3 rayDir = mad( jitteredCoords.y, c_traceRayDirUp.xyz, mad( jitteredCoords.x, c_traceRayDirRight.xyz, c_traceRayDirBase.xyz ) );
	const float3 rayInvDir = rcp( rayDir );

	Hit hit = (Hit)0;
	hit.depth = HLSL_FLT_MAX;

	for ( uint page = 0; page < gs_pageCount; ++page, remainingPatchCount -= 64 )
	{
		// Load up to 64 patches in LDS
		
		{
			TracePatch tracePatch = (TracePatch)0;

			if ( group < remainingPatchCount )
			{
				const uint patchID = mad( page, pageSize, pageOffset );
				tracePatch = LoadPatch( patchID );
			}

			gs_patches[group] = tracePatch;
		}

		// Compute up to 64 visible blocks to intersect

		uint patchMask0 = BuildTraceMask( group_xMask8_yMask8, 0 );
		uint patchMask1 = BuildTraceMask( group_xMask8_yMask8, 32 );

		// Process the first serie of 32 blocks

		while ( patchMask0 )
			ExecuteTraceMask( hit, patchMask0, rayDir, rayInvDir, 0 );
		
		// Process the second serie of 32 blocks

		while ( patchMask1 )
			ExecuteTraceMask( hit, patchMask1, rayDir, rayInvDir, 32 );

#if BLOCK_DEBUG == 1
		InterlockedAdd( blockTraceStats[0].pixelPageCount, 1 );
#endif // #if BLOCK_DEBUG == 1
	}

	float3 groupColor = float3( 0.0f, 0.0f, 0.0f );

	if ( hit.depth < HLSL_FLT_MAX )
	{
		// Decode the BC1 color of the nearest hit

		// 100 us

		uint64 colorIndices64;
		if ( hit.cellRank < 32 )
			colorIndices64 = blockCellColorIndices0[hit.blockPosID];
		else
			colorIndices64 = blockCellColorIndices1[hit.blockPosID];

		const bool useHighBits = (hit.cellRank >> 4) & 1;
		const uint colorIndices32 = useHighBits ? colorIndices64.high : colorIndices64.low;
		const uint colorShift = (hit.cellRank & 0xF) << 1;
		const uint colorID = (colorIndices32 >> colorShift) & 3;

		const uint rgb_none = hit.blockColorPalette[colorID];

		groupColor = float3( (rgb_none >> 24) & 0xFF, (rgb_none >> 16) & 0xFF, (rgb_none >> 8) & 0xFF ) * (1.0f / 255.0f);
	}

	{
		// Ouput
		
		// 50 us

		const uint2 screenBufferPos = uint2( DTid.x, c_traceFrameSize.y - DTid.y - 1 );
		outputColors[screenBufferPos] = float4( groupColor, 1.0f );

		const uint pixelID = mad( screenBufferPos.y, c_traceFrameSize.x, screenBufferPos.x );
		outputDisplacements[pixelID] = hit.xdsp16_ydsp16;
	}
}
