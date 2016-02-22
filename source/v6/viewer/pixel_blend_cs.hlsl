#define HLSL
#include "common_shared.h"

#define BUFFER_WIDTH HLSL_PIXEL_SUPER_SAMPLING_WIDTH

StructuredBuffer< BlockCellItem > blockCellItems				: register( HLSL_BLOCK_CELL_ITEM_SRV );
Buffer< uint > firstBlockCellItemIDs							: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_SRV );

RWTexture2D< float4 > outputColors								: register( HLSL_COLOR_UAV );

uint3 UnpackRGB_NONE( uint rgb_none )
{
	return uint3( (rgb_none >> 24) & 0xFF, (rgb_none >> 16) & 0xFF, (rgb_none >> 8) & 0xFF );
}

#if BUFFER_WIDTH == 1

#define DEPTH_MAX 0xFFFFFFFF

struct Pixel_s
{
	uint depth21_none11;
	uint r8g8b8_none8;
};

struct PixelBuffer_s
{
	Pixel_s pixels[9];
};

groupshared PixelBuffer_s g_pixelBuffers[10][10];

#define UPDATE_OTHER_PIXEL( PIXELID, UPDATE, DEPTH21_NONE11, R8G8B8_NONE8 ) \
	if ( (UPDATE) != 0 && (DEPTH21_NONE11 < pixels[PIXELID].depth21_none11) ) \
	{ \
		pixels[PIXELID].depth21_none11 = DEPTH21_NONE11; \
		pixels[PIXELID].r8g8b8_none8 = R8G8B8_NONE8; \
	}

#define UPDATE_THIS_PIXEL( OTHER_X, OTHER_Y, OTHER_PIXEL_ID ) \
	{ \
		const uint2 otherGroupID = groupID + int2( OTHER_X, OTHER_Y ); \
		const uint depth21_none11 = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].depth21_none11; \
		const uint r8g8b8_none8 = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].r8g8b8_none8; \
		if ( depth21_none11 < thisDepth21_none11 ) \
		{ \
			thisDepth21_none11 = depth21_none11; \
			thisR8G8B8_none8 = r8g8b8_none8; \
		} \
	}

void SamplePixel( int2 bufferBasePos, uint2 groupID )
{
	Pixel_s pixels[9];

	for ( int hitShift = 0; hitShift < 9; ++hitShift )
	{
		pixels[hitShift].depth21_none11 = DEPTH_MAX;
		pixels[hitShift].r8g8b8_none8 = 0;
	}

	const int2 bufferPos = bufferBasePos + groupID;
	if ( bufferPos.x >= 0 && bufferPos.x < (int)c_pixelFrameSize.x && bufferPos.y >= 0 && bufferPos.y < (int)c_pixelFrameSize.y )
	{		
		const uint pixelID = bufferPos.y * c_pixelFrameSize.x + bufferPos.x;
		const uint blockCellItemBucket = mad( bufferPos.y >> 3, c_pixelFrameSize.x >> 3, bufferPos.x >> 3 );
		const uint blockCellSubBucketMaxCount = (c_pixelFrameSize.x >> 3) * (c_pixelFrameSize.y >> 3);
		uint blockCellItemID = firstBlockCellItemIDs[pixelID];		
		uint step = 0;
		while ( blockCellItemID != 0 )
		{
			const uint blockCellItemPage = blockCellItemID >> 8;
			const uint blockCellItemSubID = blockCellItemID & 0xFF;
			const uint blockCellItemOffset = mad( blockCellItemPage, blockCellSubBucketMaxCount, blockCellItemBucket ) * HLSL_CELL_ITEM_PER_SUB_BUCKET_MAX_COUNT;
			const uint blockCellItemAddress = blockCellItemOffset + blockCellItemSubID;
			const uint depth21_nextID11 = blockCellItems[blockCellItemAddress].depth21_nextID11;
			const uint r8g8b8_hitMask8 = blockCellItems[blockCellItemAddress].r8g8b8_hitMask8;

			UPDATE_OTHER_PIXEL( 0, r8g8b8_hitMask8 & 0x01, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 1, r8g8b8_hitMask8 & 0x02, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 2, r8g8b8_hitMask8 & 0x04, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 3, r8g8b8_hitMask8 & 0x08, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 4, r8g8b8_hitMask8 & 0x10, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 5, r8g8b8_hitMask8 & 0x20, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 6, r8g8b8_hitMask8 & 0x40, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 7, r8g8b8_hitMask8 & 0x80, depth21_nextID11, r8g8b8_hitMask8 );
			UPDATE_OTHER_PIXEL( 8, true, depth21_nextID11, r8g8b8_hitMask8 );

			blockCellItemID = depth21_nextID11 & 0x7FF;
			++step;
		}
	}

	g_pixelBuffers[groupID.y][groupID.x].pixels = pixels;
}

float3 Blend( uint3 DTid, uint3 GTid )
{
	const uint2 groupID = GTid.xy + 1;
	const int2 bufferPos = DTid.xy;
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

	uint thisDepth21_none11 = g_pixelBuffers[groupID.y][groupID.x].pixels[8].depth21_none11;
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

float3 ComputeOverdrawColor( uint3 DTid )
{
	const int2 bufferPos = DTid.xy;

	const uint pixelID = bufferPos.y * c_pixelFrameSize.x + bufferPos.x;
	const uint blockCellItemBucket = mad( bufferPos.y >> 3, c_pixelFrameSize.x >> 3, bufferPos.x >> 3 );
	const uint blockCellSubBucketMaxCount = (c_pixelFrameSize.x >> 3) * (c_pixelFrameSize.y >> 3);
	uint blockCellItemID = firstBlockCellItemIDs[pixelID];		
	uint step = 0;
	while ( blockCellItemID != 0 )
	{
		const uint blockCellItemPage = blockCellItemID >> 8;
		const uint blockCellItemSubID = blockCellItemID & 0xFF;
		const uint blockCellItemOffset = mad( blockCellItemPage, blockCellSubBucketMaxCount, blockCellItemBucket ) * HLSL_CELL_ITEM_PER_SUB_BUCKET_MAX_COUNT;
		const uint blockCellItemAddress = blockCellItemOffset + blockCellItemSubID;
		blockCellItemID = blockCellItems[blockCellItemAddress].depth21_nextID11 & 0x7FF;
		++step;
	}
	
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

float3 WarmGPU( uint3 DTid )
{
	float3 color = float3( 0.0f, 0.0f, 0.0f );
	for ( uint x = 0; x < DTid.x; x += 8 )
		for ( uint y = 0; y < DTid.y; y += 8 )
			color += x * x + y * y;
	return color;
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
	const uint2 screenPos = uint2( DTid.x, c_pixelFrameSize.y - DTid.y - 1 );
#if 1
	const float3 color = Blend( DTid, GTid );
#elif 0
	const float3 color = ComputeOverdrawColor( DTid );
#else
	const float3 color = WarmGPU( DTid );
#endif
	outputColors[screenPos] = float4( color, 1.0f );
}

#endif // #if BUFFER_WIDTH == 1
