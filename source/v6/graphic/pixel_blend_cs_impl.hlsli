#define HLSL
#include "trace_shared.h"

StructuredBuffer< BlockCellItem > blockCellItems				: REGISTER_SRV( HLSL_BLOCK_CELL_ITEM_SLOT );
Buffer< uint > cellItemCounters									: REGISTER_SRV( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT );

RWTexture2D< float4 > outputColors								: REGISTER_UAV( HLSL_COLOR_SLOT );

uint3 UnpackRGB_NONE( uint rgb_none )
{
	return uint3( (rgb_none >> 24) & 0xFF, (rgb_none >> 16) & 0xFF, (rgb_none >> 8) & 0xFF );
}

#define DEPTH_MAX 3.40282347E+38f

struct Pixel_s
{
	float depth;
	uint r8g8b8_none8;
};

struct PixelBuffer_s
{
	Pixel_s pixels[9];
};

groupshared PixelBuffer_s g_pixelBuffers[10][10];

#define UPDATE_OTHER_PIXEL( PIXELID, UPDATE, DEPTH, R8G8B8_NONE8 ) \
	if ( (UPDATE) != 0 && (DEPTH < pixels[PIXELID].depth) ) \
	{ \
		pixels[PIXELID].depth = DEPTH; \
		pixels[PIXELID].r8g8b8_none8 = R8G8B8_NONE8; \
	}

#define UPDATE_THIS_PIXEL( OTHER_X, OTHER_Y, OTHER_PIXEL_ID ) \
	{ \
		const uint2 otherGroupID = groupID + int2( OTHER_X, OTHER_Y ); \
		const float depth = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].depth; \
		const uint r8g8b8_none8 = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].r8g8b8_none8; \
		if ( depth < thisDepth ) \
		{ \
			thisDepth = depth; \
			thisR8G8B8_none8 = r8g8b8_none8; \
		} \
	}

void SamplePixel( int2 bufferBasePos, uint2 groupID )
{
	Pixel_s pixels[9];

	for ( int hitShift = 0; hitShift < 9; ++hitShift )
	{
		pixels[hitShift].depth = DEPTH_MAX;
		pixels[hitShift].r8g8b8_none8 = 0;
	}

	const int2 bufferPos = bufferBasePos + groupID;
	const int2 pixelCoords = int2( bufferPos.x - c_pixelEye * c_pixelFrameSize.x, bufferPos.y );
	if ( pixelCoords.x >= 0 && pixelCoords.x < (int)c_pixelFrameSize.x && pixelCoords.y >= 0 && pixelCoords.y < (int)c_pixelFrameSize.y )
	{		
		const int cellItemCountPerPage = c_pixelFrameSize.x * c_pixelEyeCount * c_pixelFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
		const uint pixelID = (mad( bufferPos.y >> 3, (c_pixelFrameSize.x * c_pixelEyeCount) >> 3, bufferPos.x >> 3 ) << 6) + mad( bufferPos.y & 7, 8, bufferPos.x & 7 );
		const uint blockCellItemCount = min( cellItemCounters[pixelID], HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT );
		for ( uint blockCellItemRank = 0; blockCellItemRank < blockCellItemCount; ++blockCellItemRank )
		{
			const uint blockCellItemPage = blockCellItemRank >> HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT;
			const uint blockCellItemRankInPage = blockCellItemRank & HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK;
			const uint blockCellItemID = blockCellItemPage * cellItemCountPerPage + HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * pixelID + blockCellItemRankInPage;
			const float depth = blockCellItems[blockCellItemID].depth;
			const uint r8g8b8_hitMask8 = blockCellItems[blockCellItemID].r8g8b8_hitMask8;

			UPDATE_OTHER_PIXEL( 0, r8g8b8_hitMask8 & 0x01, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 1, r8g8b8_hitMask8 & 0x02, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 2, r8g8b8_hitMask8 & 0x04, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 3, r8g8b8_hitMask8 & 0x08, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 4, r8g8b8_hitMask8 & 0x10, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 5, r8g8b8_hitMask8 & 0x20, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 6, r8g8b8_hitMask8 & 0x40, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 7, r8g8b8_hitMask8 & 0x80, depth, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 8, true, depth, r8g8b8_hitMask8 );
		}
	}

	g_pixelBuffers[groupID.y][groupID.x].pixels = pixels;
}

float3 Blend( uint2 bufferPos, uint3 GTid )
{
	const uint2 groupID = GTid.xy + 1;
	const int2 bufferBasePos = bufferPos - groupID;	
	
	// Sample this pixel
		
	SamplePixel( bufferBasePos, groupID );
	
	// Sample an external pixel at the boundary of the 8x8 grid

	if ( GTid.y < 4 || GTid.x == 0 )
	{		
		uint2 borderGroupID;
		switch ( GTid.y )
		{
		case 0:
			borderGroupID = uint2( 0, groupID.x );
			break;
		case 1:
			borderGroupID = uint2( 9, groupID.x );
			break;
		case 2:
			borderGroupID = uint2( groupID.x, 0 );
			break;
		case 3:
			borderGroupID = uint2( groupID.x, 9 );
			break;
		case 4:
			borderGroupID = uint2( 0, 0 );
			break;
		case 5:
			borderGroupID = uint2( 9, 0 );
			break;
		case 6:
			borderGroupID = uint2( 0, 9 );
			break;
		case 7:
			borderGroupID = uint2( 9, 9 );
			break;
		};

		SamplePixel( bufferBasePos, borderGroupID );
	}
		
	AllMemoryBarrierWithGroupSync();

	float thisDepth = g_pixelBuffers[groupID.y][groupID.x].pixels[8].depth;
	uint thisR8G8B8_none8 = g_pixelBuffers[groupID.y][groupID.x].pixels[8].r8g8b8_none8;

	UPDATE_THIS_PIXEL( -1, -1, 7 );
	UPDATE_THIS_PIXEL(  0, -1, 6 );
	UPDATE_THIS_PIXEL( +1, -1, 5 );
	UPDATE_THIS_PIXEL( -1,  0, 4 );
	UPDATE_THIS_PIXEL( +1,  0, 3 );
	UPDATE_THIS_PIXEL( -1, +1, 2 );
	UPDATE_THIS_PIXEL(  0, +1, 1 );
	UPDATE_THIS_PIXEL( +1, +1, 0 );	
		
	const float3 color = UnpackRGB_NONE( thisR8G8B8_none8 ) / 255.0f;
	return color;
}

float3 ComputeOverdrawColor( uint2 bufferPos )
{
	const int cellItemCountPerPage = c_pixelFrameSize.x * c_pixelFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
	const uint pixelID = (mad( bufferPos.y >> 3, (c_pixelFrameSize.x * c_pixelEyeCount) >> 3, bufferPos.x >> 3 ) << 6) + mad( bufferPos.y & 7, 8, bufferPos.x & 7 );
	const uint step = cellItemCounters[pixelID];
	
#if 1

#if 1
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
#else
	float3 color;
	if ( step < 64 )
		color = float3( 0.0f, 0.0f, 0.0f );
	else
		color = float3( 1.0f, 0.0f, 0.0f );
#endif

#else

	const float3 color = UnpackRGB_NONE( rgba ) / 255.0f;

#endif

	return color;
}

float3 WarmGPU( uint2 bufferPos )
{
	float3 color = float3( 0.0f, 0.0f, 0.0f );
	for ( uint x = 0; x < bufferPos.x; x += 8 )
		for ( uint y = 0; y < bufferPos.y; y += 8 )
			color += x * x + y * y;
	return color;
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
	const uint2 screenPos = uint2( DTid.x, c_pixelFrameSize.y - DTid.y - 1 );
	const uint2 bufferPos = uint2( DTid.x + c_pixelEye * c_pixelFrameSize.x, DTid.y );
#if PIXE_OVERDRAW == 1
	const float3 color = ComputeOverdrawColor( bufferPos );
#elif 1
	const float3 color = Blend( bufferPos, GTid );
#else
	const float3 color = WarmGPU( bufferPos );
#endif
	outputColors[screenPos] = float4( color, 1.0f );
}
