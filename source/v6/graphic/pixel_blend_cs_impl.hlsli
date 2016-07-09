#define HLSL
#include "trace_shared.h"

#define HIT_MASK_ALL		0x1FF
#define HIT_MASK_BOTTOM		((1 << 0) | (1 << 1) | (1 << 2))
#define HIT_MASK_TOP		((1 << 6) | (1 << 7) | (1 << 8))
#define HIT_MASK_LEFT		((1 << 0) | (1 << 3) | (1 << 6))
#define HIT_MASK_RIGHT		((1 << 2) | (1 << 5) | (1 << 8))

StructuredBuffer< BlockCellItem > blockCellItems	: REGISTER_SRV( HLSL_BLOCK_CELL_ITEM_SLOT );
Buffer< uint > cellItemCounters						: REGISTER_SRV( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT );

RWTexture2D< float4 > outputColors					: REGISTER_UAV( HLSL_COLOR_SLOT );
RWBuffer< uint > outputDisplacements				: REGISTER_UAV( HLSL_DISPLACEMENT_SLOT );
#if BLEND_DEBUG == 1
RWStructuredBuffer< BlendStats > blendStats			: REGISTER_UAV( HLSL_BLEND_STATS_SLOT );
#endif // #if BLEND_DEBUG == 1

#if 1

struct PixelContext_s
{
	uint pixelOffset;
	uint cellItemCount;
};

struct PixelDepth_s
{
	uint depth24_xgroup4_ygroup4;
};

struct PixelData_s
{
	uint r8g8b8_none8;
	uint xdsp16_ydsp16;
};

groupshared uint			gs_maxCellItemCount;			//    4 bytes
groupshared PixelContext_s	gs_pixelContexts[10][10];		//  800 bytes
groupshared PixelData_s		gs_pixelCandidates[10][10];		//  800 bytes
groupshared PixelDepth_s	gs_pixelDepths[10][10];			//  400 bytes
groupshared PixelData_s		gs_pixelDatas[10][10];			//  800 bytes
															// 2804 bytes <= total

uint3 UnpackRGB_NONE( uint rgb_none )
{
	return uint3( (rgb_none >> 24) & 0xFF, (rgb_none >> 16) & 0xFF, (rgb_none >> 8) & 0xFF );
}

void ComputeBorderGroupIDAndHitMask( out uint2 borderGroupID, out uint hitMask, uint2 GTid )
{
	borderGroupID = uint2( 0, 0 );
	hitMask = 0;

	if ( GTid.y < 4 || GTid.x == 0 )
	{
		const uint2 groupID = GTid + 1;

		switch ( GTid.y )
		{
		case 0:
			borderGroupID = int2( 0, groupID.x );
			hitMask = HIT_MASK_RIGHT;
			break;
		case 1:
			borderGroupID = uint2( 9, groupID.x );
			hitMask = HIT_MASK_LEFT;
			break;
		case 2:
			borderGroupID = uint2( groupID.x, 0 );
			hitMask = HIT_MASK_TOP;
			break;
		case 3:
			borderGroupID = uint2( groupID.x, 9 );
			hitMask = HIT_MASK_BOTTOM;
			break;
		case 4:
			borderGroupID = uint2( 0, 0 );
			hitMask = HIT_MASK_RIGHT | HIT_MASK_TOP;
			break;
		case 5:
			borderGroupID = uint2( 9, 0 );
			hitMask = HIT_MASK_LEFT | HIT_MASK_TOP;
			break;
		case 6:
			borderGroupID = uint2( 0, 9 );
			hitMask = HIT_MASK_RIGHT | HIT_MASK_BOTTOM;
			break;
		case 7:
			borderGroupID = uint2( 9, 9 );
			hitMask = HIT_MASK_LEFT | HIT_MASK_BOTTOM;
			break;
		};
	}
}

void InitPixelContext( int2 bufferBasePos, uint2 groupID )
{
	PixelContext_s pixelContext;
	pixelContext.pixelOffset = 0;
	pixelContext.cellItemCount = 0;

	const int2 bufferPos = bufferBasePos + groupID;
	const uint2 pixelCoords = uint2( bufferPos.x - c_blendEye * c_blendFrameSize.x, bufferPos.y );
	if ( pixelCoords.x < (uint)c_blendFrameSize.x && pixelCoords.y < (uint)c_blendFrameSize.y )
	{
		const uint2 cellBufferSize = uint2( c_blendFrameSize.x * c_blendEyeCount, c_blendFrameSize.y );
		const uint pageSize = (cellBufferSize.x * cellBufferSize.y) << HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT;
		const uint bw = cellBufferSize.x >> 3;
		const uint bh = cellBufferSize.y >> 3;
		const uint bx = bufferPos.x >> 3;
		const uint by = bufferPos.y >> 3;
		const uint gx = bufferPos.x & 7;
		const uint gy = bufferPos.y & 7;

		const uint tileOffset = (by * bw + bx) << 6;
		const uint groupOffset = (gy << 3) + gx;

		const uint cellCounterID = tileOffset + groupOffset;

		pixelContext.pixelOffset = (tileOffset << HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT) + groupOffset;
		pixelContext.cellItemCount = min( cellItemCounters[cellCounterID], HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT );
	}

	gs_pixelContexts[groupID.y][groupID.x] = pixelContext;
	InterlockedMax( gs_maxCellItemCount, pixelContext.cellItemCount );
}

void ClearPixelDataAndDepth( uint2 groupID )
{
	PixelData_s pixelData;
	pixelData.r8g8b8_none8 = 0;
	pixelData.xdsp16_ydsp16 = 0;
	gs_pixelDatas[groupID.y][groupID.x] = pixelData;

	gs_pixelDepths[groupID.y][groupID.x].depth24_xgroup4_ygroup4 = 0xFFFFFFFF;
}

void LoadPixelCandidate( int2 bufferBasePos, uint2 groupID, uint hitMask, uint cellItemRank, uint cellItemCountPerPage )
{
	PixelContext_s pixelContext = gs_pixelContexts[groupID.y][groupID.x];

#if BLEND_DEBUG == 1
	bool isBorder = hitMask != HIT_MASK_ALL;
	if ( c_blendGetStats )
	{
		if ( isBorder )
			InterlockedAdd( blendStats[0].cellItemBorderInputCount, 1 );
		else
			InterlockedAdd( blendStats[0].cellItemHitInputCount, 1 );
	}
#endif // #if BLEND_DEBUG == 1

	if ( cellItemRank < pixelContext.cellItemCount )
	{
#if BLEND_DEBUG == 1
		if ( c_blendGetStats )
		{
			if ( isBorder )
				InterlockedAdd( blendStats[0].cellItemBorderProcessedCount, 1 );
			else
				InterlockedAdd( blendStats[0].cellItemHitProcessedCount, 1 );
		}
#endif // #if BLEND_DEBUG == 1

		const uint page = cellItemRank >> HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT;
		const uint plane = cellItemRank & HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK;
		const uint blockCellItemID = page * cellItemCountPerPage + (plane << 6) + pixelContext.pixelOffset;
		const BlockCellItem blockCellItem = blockCellItems[blockCellItemID];

		PixelData_s pixelCandidate;
		pixelCandidate.r8g8b8_none8 = blockCellItem.r8g8b8_hitMask8;
		pixelCandidate.xdsp16_ydsp16 = blockCellItem.xdsp16_ydsp16;
		gs_pixelCandidates[groupID.y][groupID.x] = pixelCandidate;

		const uint hitMask9 = (((blockCellItem.hitMask1_depth31 >> 23) & 0x100) | (blockCellItem.r8g8b8_hitMask8 & 0xFF)) & hitMask;
		const uint depth24_xgroup4_ygroup4 = (uint( asfloat( blockCellItem.hitMask1_depth31 & 0x7FFFFFFF ) * c_blendDepth24Norm ) << 8) | (groupID.x << 4) | groupID.y;

		if ( hitMask9 & (1 << 0) ) InterlockedMin( gs_pixelDepths[groupID.y-1][groupID.x-1].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
		if ( hitMask9 & (1 << 1) ) InterlockedMin( gs_pixelDepths[groupID.y-1][groupID.x  ].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
		if ( hitMask9 & (1 << 2) ) InterlockedMin( gs_pixelDepths[groupID.y-1][groupID.x+1].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );

		if ( hitMask9 & (1 << 3) ) InterlockedMin( gs_pixelDepths[groupID.y  ][groupID.x-1].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
		if ( hitMask9 & (1 << 4) ) InterlockedMin( gs_pixelDepths[groupID.y  ][groupID.x  ].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
		if ( hitMask9 & (1 << 5) ) InterlockedMin( gs_pixelDepths[groupID.y  ][groupID.x+1].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );

		if ( hitMask9 & (1 << 6) ) InterlockedMin( gs_pixelDepths[groupID.y+1][groupID.x-1].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
		if ( hitMask9 & (1 << 7) ) InterlockedMin( gs_pixelDepths[groupID.y+1][groupID.x  ].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
		if ( hitMask9 & (1 << 8) ) InterlockedMin( gs_pixelDepths[groupID.y+1][groupID.x+1].depth24_xgroup4_ygroup4, depth24_xgroup4_ygroup4 );
	}
}

void MergePixelData( uint2 groupID )
{
	const uint xgroup4_ygroup4 = gs_pixelDepths[groupID.y][groupID.x].depth24_xgroup4_ygroup4 & 0xFF;
	if ( xgroup4_ygroup4 != 0xFF )
	{
		const uint2 candidateGroupID = uint2( xgroup4_ygroup4 >> 4, xgroup4_ygroup4 & 0xF );
		gs_pixelDatas[groupID.y][groupID.x] = gs_pixelCandidates[candidateGroupID.y][candidateGroupID.x];
	}

	gs_pixelDepths[groupID.y][groupID.x].depth24_xgroup4_ygroup4 |= 0xFF;
}

void OuputPixelData( out float3 out_color, out uint out_displacement, uint2 groupID )
{
	const PixelData_s pixelData = gs_pixelDatas[groupID.y][groupID.x];
	out_color = UnpackRGB_NONE( pixelData.r8g8b8_none8 ) / 255.0f;
	out_displacement = pixelData.xdsp16_ydsp16;
}

void Blend( out float3 out_color, out uint out_displacement, uint2 bufferPos, uint2 GTid )
{
	if ( GTid.x == 0 && GTid.y == 0 )
		gs_maxCellItemCount = 0;

	const int cellItemCountPerPage = c_blendFrameSize.x * c_blendEyeCount * c_blendFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
	const uint2 groupID = GTid + 1;
	const int2 bufferBasePos = bufferPos - groupID;

	ClearPixelDataAndDepth( groupID );

	uint2 borderGroupID;
	uint borderHitMask;
	ComputeBorderGroupIDAndHitMask( borderGroupID, borderHitMask, GTid );

	GroupMemoryBarrierWithGroupSync(); // ensure gs_maxCellItemCount == 0

	InitPixelContext( bufferBasePos, groupID );
	if ( borderHitMask )
		InitPixelContext( bufferBasePos, borderGroupID );

	GroupMemoryBarrierWithGroupSync(); // ensure gs_maxCellItemCount is set

	for ( uint cellItemRank = 0; cellItemRank < gs_maxCellItemCount; ++cellItemRank )
	{
		LoadPixelCandidate( bufferBasePos, groupID, HIT_MASK_ALL, cellItemRank, cellItemCountPerPage );
		if ( borderHitMask )
			LoadPixelCandidate( bufferBasePos, borderGroupID, borderHitMask, cellItemRank, cellItemCountPerPage );

		GroupMemoryBarrierWithGroupSync(); // ensure that all pixel candidates are loaded

		MergePixelData( groupID );

		GroupMemoryBarrierWithGroupSync(); // ensure that all pixel data are merged
	}

	OuputPixelData( out_color, out_displacement, groupID );
}

#else

void Blend( out float3 out_color, out uint out_displacement, uint2 bufferPos, uint2 GTid )
{
	const int cellItemCountPerPage = c_blendFrameSize.x * c_blendEyeCount * c_blendFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;

	const uint2 pixelCoords = uint2( bufferPos.x - c_blendEye * c_blendFrameSize.x, bufferPos.y );
	const uint pixelID = (mad( bufferPos.y >> 3, (c_blendFrameSize.x * c_blendEyeCount) >> 3, bufferPos.x >> 3 ) << 6) + mad( bufferPos.y & 7, 8, bufferPos.x & 7 );
	const uint cellItemCount = min( cellItemCounters[pixelID], HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT );

	uint d = 0;

	for ( uint cellItemRank = 0; cellItemRank < cellItemCount; ++cellItemRank )
	{
		const uint blockCellItemPage = cellItemRank >> HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT;
		const uint blockCellItemRankInPage = cellItemRank & HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK;
		const uint blockCellItemID = blockCellItemPage * cellItemCountPerPage + HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * pixelID + blockCellItemRankInPage;
		const BlockCellItem blockCellItem = blockCellItems[blockCellItemID];

		const uint hitMask9 = ((blockCellItem.hitMask1_depth31 >> 23) & 0x100) | (blockCellItem.r8g8b8_hitMask8 & 0xFF);
		const uint depth24_cellItemRank8 = (uint( asfloat( blockCellItem.hitMask1_depth31 & 0x7FFFFFFF ) * c_blendDepth24Norm ) << 8) | cellItemRank;

		d += depth24_cellItemRank8 & hitMask9;
	}

	out_color = float3( 0.0f, 0.0f, 0.0f );
	out_displacement = d;
}

#endif

float3 ComputeOverdrawColor( uint2 bufferPos )
{
	const int cellItemCountPerPage = c_blendFrameSize.x * c_blendFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
	const uint pixelID = (mad( bufferPos.y >> 3, (c_blendFrameSize.x * c_blendEyeCount) >> 3, bufferPos.x >> 3 ) << 6) + mad( bufferPos.y & 7, 8, bufferPos.x & 7 );
	const uint step = cellItemCounters[pixelID];
	
	float3 color;
	if ( step < 1 )
		color = float3( 0.0f, 0.0f, 0.0f );
	else if ( step < 2 )
		color = float3( 0.0f, 0.0f, 1.0f );
	else if ( step < 4 )
		color = float3( 0.0f, 0.5f, 0.5f );
	else if ( step < 8 )
		color = float3( 0.0f, 1.0f, 0.0f );
	else if ( step < 16 )
		color = float3( 0.5f, 0.5f, 0.0f );
	else
		color = float3( 1.0f, 0.0f, 0.0f );

	return color;
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
	const uint2 screenBufferPos = uint2( DTid.x, c_blendFrameSize.y - DTid.y - 1 );
	const uint2 stereoBuffPos = uint2( DTid.x + c_blendEye * c_blendFrameSize.x, DTid.y );

	const uint pixelID = mad( screenBufferPos.y, c_blendFrameSize.x, screenBufferPos.x );

	float3 color = float3( 0.0f, 0.0f, 0.0f );
	uint displacement = 0;

#if BLEND_DEBUG == 1
	if ( c_blendGetStats )
		InterlockedAdd( blendStats[0].pixelInputCount, 1 );

	if ( c_blendShowOverdraw )
	{
		color = ComputeOverdrawColor( stereoBuffPos );
	}
	else
#endif // #if BLEND_DEBUG == 1
	{
		Blend( color, displacement, stereoBuffPos, GTid.xy );
	}

	outputColors[screenBufferPos] = float4( color, 1.0f );
	outputDisplacements[pixelID] = displacement;
}
