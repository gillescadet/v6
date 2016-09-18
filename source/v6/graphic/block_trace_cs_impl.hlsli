#define HLSL

#include "trace_shared.h"
#include "onion_impl.hlsli"

Buffer< uint > blockPatchCounters						: REGISTER_SRV( HLSL_BLOCK_PATCH_COUNTERS_SLOT );
#if TRACE_ONION == 1
StructuredBuffer< BlockPatchOnion > blockPatches		: REGISTER_SRV( HLSL_BLOCK_PATCHES_SLOT );
#else
StructuredBuffer< BlockPatchMip > blockPatches			: REGISTER_SRV( HLSL_BLOCK_PATCHES_SLOT );
#endif
Buffer< uint > blockCellPresences0						: REGISTER_SRV( HLSL_BLOCK_CELL_PRESENCE0_SLOT );
Buffer< uint > blockCellPresences1						: REGISTER_SRV( HLSL_BLOCK_CELL_PRESENCE1_SLOT );
Buffer< uint > blockCellEndColors						: REGISTER_SRV( HLSL_BLOCK_CELL_END_COLOR_SLOT );
Buffer< uint > blockCellColorIndices0					: REGISTER_SRV( HLSL_BLOCK_CELL_COLOR_INDEX0_SLOT );
Buffer< uint > blockCellColorIndices1					: REGISTER_SRV( HLSL_BLOCK_CELL_COLOR_INDEX1_SLOT );
Buffer< uint > blockCellColorIndices2					: REGISTER_SRV( HLSL_BLOCK_CELL_COLOR_INDEX2_SLOT );
Buffer< uint > blockCellColorIndices3					: REGISTER_SRV( HLSL_BLOCK_CELL_COLOR_INDEX3_SLOT );

RWTexture2D< float4 > outputColors						: REGISTER_UAV( HLSL_COLOR_SLOT );
RWBuffer< uint > outputDisplacements					: REGISTER_UAV( HLSL_DISPLACEMENT_SLOT );
#if BLOCK_DEBUG == 1
RWStructuredBuffer< BlockTraceStats > blockTraceStats	: REGISTER_UAV( HLSL_TRACE_STATS_SLOT );
RWStructuredBuffer< BlockDebugBox > blockDebugBoxes		: REGISTER_UAV( HLSL_TRACE_DEBUG_BOX_SLOT );
#endif // #if BLOCK_DEBUG == 1

struct TracePatch
{
	uint	blockPosID;
	uint	grid4_newBlock1_none11_xMask8_yMask8; // optim: separate this and merge two patches
	uint3	cellMinRange;
	uint3	cellExtent;
	float3	boxMinRS;
	float3	boxMaxRS;
#if TRACE_ONION == 1
	float3	cellSize;
	float3	invCellSize;
#else
	float	cellSize;
	float	invCellSize;
#endif
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
	uint	newBlock;
#if BLOCK_DEBUG == 1
	uint	grid;
	uint	traceCount;
	bool	debug;
#endif // #if BLOCK_DEBUG == 1
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
#if TRACE_ONION == 1
	const BlockPatchOnion blockPatch = blockPatches[patchID];
#else
	const BlockPatchMip blockPatch = blockPatches[patchID];
#endif

	// Compute bounding box

	const uint newBlock = (blockPatch.newBlock1_blockPosID >> 31) & 1;
	const uint blockPosID = blockPatch.newBlock1_blockPosID & 0x7FFFFFFF;

	uint3 cellMinRange;
	cellMinRange.x = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 26) & 3;
	cellMinRange.y = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 24) & 3;
	cellMinRange.z = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 22) & 3;
			
	uint3 cellMaxRange;
	cellMaxRange.x = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 20) & 3;
	cellMaxRange.y = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 18) & 3;
	cellMaxRange.z = (blockPatch.none4_cellmin222_cellmax222_x4_y4_w4_h4 >> 16) & 3;

	const float3 cellExtent = cellMaxRange - cellMinRange;

#if TRACE_ONION == 1
	const uint sign = (blockPatch.sign1_axis2_z11_y9_x9 >> 31) & 1;
	const uint axis = (blockPatch.sign1_axis2_z11_y9_x9 >> 29) & 3;
	const uint grid = mad( sign, 3, axis );
	uint3 blockCoords;
	blockCoords.x = (blockPatch.sign1_axis2_z11_y9_x9 >>  0) & 0x1FF;
	blockCoords.y = (blockPatch.sign1_axis2_z11_y9_x9 >>  9) & 0x1FF;
	blockCoords.z = (blockPatch.sign1_axis2_z11_y9_x9 >> 18) & 0x7FF;

	float3 blockPosMinRS, blockPosMaxRS;
	Onion_BlockCoordsToPos( blockPosMinRS, blockPosMaxRS, sign, axis, blockCoords, c_traceGridMinScale, c_traceInvMacroPeriodWidth, c_traceInvMacroGridWidth );

	const float3 cellSize = (blockPosMaxRS - blockPosMinRS) * 0.25f;
	const float3 invCellSize = rcp( cellSize );
	const float3 blockOrgRS = blockPosMinRS + c_traceGridCenter.xyz - c_traceRayOrg.xyz;
	const float3 boxMinRS = mad( cellMinRange, cellSize, blockOrgRS );
	const float3 boxMaxRS = mad( float3( cellMaxRange + 1.0f ), cellSize, blockOrgRS );
#else // #if TRACE_ONION == 1
	const uint grid = blockPatch.mip4_none1_blockPos27 >> 28;
	const uint blockPos = blockPatch.mip4_none1_blockPos27 & 0x07FFFFFF;
	const uint blockPosX = (blockPos >> (HLSL_MIP_MACRO_XYZ_BIT_COUNT*0)) & HLSL_MIP_MACRO_XYZ_BIT_MASK;
	const uint blockPosY = (blockPos >> (HLSL_MIP_MACRO_XYZ_BIT_COUNT*1)) & HLSL_MIP_MACRO_XYZ_BIT_MASK;
	const uint blockPosZ = (blockPos >> (HLSL_MIP_MACRO_XYZ_BIT_COUNT*2)) & HLSL_MIP_MACRO_XYZ_BIT_MASK;

	const float gridScale = c_traceGridScales[grid].x;
	const float cellSize = c_traceGridScales[grid].y;
	const float invCellSize = c_traceGridScales[grid].z;
	const uint3 cellCoords = uint3( blockPosX, blockPosY, blockPosZ ) << 2;
	const uint3 cellMinCoords = cellCoords + cellMinRange;
	const float3 posMinWS = mad( cellMinCoords, cellSize, -gridScale ) + c_traceGridCenters[grid].xyz;

	const float3 boxMinRS = posMinWS - c_traceRayOrg.xyz; 
	const float3 boxMaxRS = mad( float3( cellExtent + 1.0f ), cellSize, boxMinRS );
#endif // #if TRACE_ONION == 0

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

	const uint cellEndColors = blockCellEndColors[blockPosID];
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
	tracePatch.blockPosID = blockPosID;
	tracePatch.grid4_newBlock1_none11_xMask8_yMask8 = (grid << 28) | (newBlock << 27) | (xMask << 8) | yMask;
	tracePatch.cellMinRange = cellMinRange;
	tracePatch.cellExtent = cellExtent;
	tracePatch.boxMinRS = boxMinRS;
	tracePatch.boxMaxRS = boxMaxRS;
	tracePatch.cellSize = cellSize;
	tracePatch.invCellSize = invCellSize;
	tracePatch.blockCellPresence.low = blockCellPresences0[blockPosID];
	tracePatch.blockCellPresence.high = blockCellPresences1[blockPosID];
	tracePatch.blockColorPalette = colorPalette; // optim: could be computed at the end in order to save LDS
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
#if BLOCK_DEBUG == 1
		hit.grid = patch.grid4_newBlock1_none11_xMask8_yMask8 >> 28;
#endif // #if BLOCK_DEBUG == 1
		hit.newBlock = (patch.grid4_newBlock1_none11_xMask8_yMask8 >> 27) & 1;
	}

#if BLOCK_DEBUG == 1
	++hit.traceCount;
	if ( hit.debug )
	{
		uint debugRayID;
		InterlockedAdd( blockTraceStats[0].debugBoxCount, 1, debugRayID );
		blockDebugBoxes[debugRayID].boxMinRS = patch.boxMinRS;
		blockDebugBoxes[debugRayID].boxMaxRS = patch.boxMaxRS;
	}
#endif // #if BLOCK_DEBUG == 1

	return hitCellID;
}

uint BuildTraceMask( uint group_xMask8_yMask8, uint patchOffset )
{
	uint patchMask = 0;
	for ( uint patchBit = 0; patchBit < 32; ++patchBit )
		patchMask |= ((group_xMask8_yMask8 & gs_patches[patchOffset + patchBit].grid4_newBlock1_none11_xMask8_yMask8) == group_xMask8_yMask8) << patchBit;

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
#if BLOCK_DEBUG == 1
	hit.debug = false;
	if ( hit.debug )
		blockTraceStats[0].debugRayDir = rayDir;
#endif

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
	float responseFactor = 1.0f;

	if ( hit.depth < HLSL_FLT_MAX )
	{
		// Decode the BC1 color of the nearest hit

		// 100 us

		uint64 colorIndices64;
		if ( hit.cellRank < 32 )
		{
			colorIndices64.low = blockCellColorIndices0[hit.blockPosID];
			colorIndices64.high = blockCellColorIndices1[hit.blockPosID];
		}
		else
		{
			colorIndices64.low = blockCellColorIndices2[hit.blockPosID];
			colorIndices64.high = blockCellColorIndices3[hit.blockPosID];
		}

		const bool useHighBits = (hit.cellRank >> 4) & 1;
		const uint colorIndices32 = useHighBits ? colorIndices64.high : colorIndices64.low;
		const uint colorShift = (hit.cellRank & 0xF) << 1;
		const uint colorID = (colorIndices32 >> colorShift) & 3;

		const uint rgb_none = hit.blockColorPalette[colorID];

		groupColor = float3( (rgb_none >> 24) & 0xFF, (rgb_none >> 16) & 0xFF, (rgb_none >> 8) & 0xFF ) * (1.0f / 255.0f);
		responseFactor = hit.newBlock ? 1.0f : 0.0f;

#if BLOCK_DEBUG == 1
		if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_GRIDS )
		{
			const float3 gridColors[] = { float3( 1.0f, 0.0f, 0.0f ), float3( 0.0f, 1.0f, 0.0f ), float3( 0.0f, 0.0f, 1.0f ) };
			groupColor = gridColors[hit.grid % 3];
			groupColor *= 1.0f / ( 1.0f + float( hit.grid / 3 ) );
		}
		else if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_HISTORY )
		{
			groupColor += hit.newBlock ? float3( 0.5f, 0.0f, 0.0f ) : float3( 0.0f, 0.f, 0.0f );
		}
		else if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_BLOCK )
		{
			const float gridScaleMax = c_traceGridScales[HLSL_MIP_MAX_COUNT-1].x;
			const float alpha = sqrt( hit.depth / gridScaleMax );
			const float3 blockColor = U32ToColor( hit.blockPosID );
			groupColor = blockColor * 0.50f + alpha * 0.50f;

		}
#endif
	}

#if BLOCK_DEBUG == 1
	if ( c_traceShowFlag & HLSL_BLOCK_SHOW_FLAG_OVERDRAW )
	{
		const uint gradient[] = { 0x00000000, 0x00FF0000, 0x22FF0000, 0x44FF0000, 0x65FF0000, 0x88FF0000, 0xAAFF0000, 0xCCFF0000, 0xEEFF0000, 0xFFED0000, 0xFFCC0000, 0xFFAA0000, 0xFF880000, 0xFF660000, 0xFF430000, 0xFF210000, 0xFF000000 };
		const uint rgba = gradient[min( hit.traceCount, 16 )];
		groupColor = float3( ((rgba >> 24) & 0xFF) / 255.0f, ((rgba >> 16) & 0xFF) / 255.0f, ((rgba >> 8) & 0xFF) / 255.0f );
	}
#endif

	{
		// Ouput
		
		// 50 us

		const uint2 screenBufferPos = uint2( DTid.x, c_traceFrameSize.y - DTid.y - 1 );
		outputColors[screenBufferPos] = float4( groupColor, responseFactor );

		const uint pixelID = mad( screenBufferPos.y, c_traceFrameSize.x, screenBufferPos.x );
		outputDisplacements[pixelID] = hit.xdsp16_ydsp16;
	}
}
