#define HLSL
#include "common_shared.h"

#define BUFFER_WIDTH HLSL_PIXEL_SUPER_SAMPLING_WIDTH

StructuredBuffer< BlockCellItem > blockCellItems				: register( HLSL_BLOCK_CELL_ITEM_SRV );
Buffer< uint > firstBlockCellItemIDs							: register( HLSL_BLOCK_FIRST_CELL_ITEM_ID_SRV );

RWTexture2D< float4 > outputColors								: register( HLSL_COLOR_UAV );
#if HLSL_DEBUG_PIXEL == 1
RWStructuredBuffer< PixelBlendDebugBuffer > debugBuffers		: register( HLSL_PIXEL_DEBUG_UAV );
#endif // #if HLSL_DEBUG_PIXEL == 1

#define debugBuffer debugBuffers[0]

uint3 UnpackRGBA( uint rgba )
{
	return uint3( (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF );
}

#if BUFFER_WIDTH == 1

#define DEPTH_MAX 0xFFFFFFFF

struct Pixel_s
{
	uint depth23_hitMask9;
	uint r8g8b8a8;
};

struct PixelBuffer_s
{
	Pixel_s pixels[9];
};

groupshared PixelBuffer_s g_pixelBuffers[10][10];

void SamplePixel( int2 bufferBasePos, uint2 groupID )
{
	Pixel_s pixels[9];

	for ( int hitShift = 0; hitShift < 9; ++hitShift )
	{
		pixels[hitShift].depth23_hitMask9 = DEPTH_MAX;
		pixels[hitShift].r8g8b8a8 = 0;
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
			const uint depth23_hitMask9 = blockCellItems[blockCellItemAddress].depth23_hitMask9;
			const uint r8g8b8a8 = blockCellItems[blockCellItemAddress].r8g8b8a8;			
			for ( uint hitShift = 0; hitShift < 9; ++hitShift )
			{
				const uint hitMask = 1 << hitShift;
				if ( (depth23_hitMask9 & hitMask) != 0 && depth23_hitMask9 < pixels[hitShift].depth23_hitMask9 )
				{
					pixels[hitShift].depth23_hitMask9 = depth23_hitMask9;
					pixels[hitShift].r8g8b8a8 = r8g8b8a8;
				}
			}
			blockCellItemID = blockCellItems[blockCellItemAddress].nextID;
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

	uint thisDepth23_hitMask9 = g_pixelBuffers[groupID.y][groupID.x].pixels[4].depth23_hitMask9;
	uint thisRGBA = g_pixelBuffers[groupID.y][groupID.x].pixels[4].r8g8b8a8;

	for ( int y = -1; y <= 1; ++y )
	{
		for ( int x = -1; x <= 1; ++x )
		{
			const uint2 otherGroupID = groupID + int2( x, y );
			const uint otherHitShift = (1 - y) * 3 + (1 - x);
			const uint depth23_hitMask9 = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[otherHitShift].depth23_hitMask9;
			if ( depth23_hitMask9 < thisDepth23_hitMask9 )
			{
				thisDepth23_hitMask9 = depth23_hitMask9;
				thisRGBA = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[otherHitShift].r8g8b8a8;
			}
		}
	}
		
	const float3 color = UnpackRGBA( thisRGBA ) / 255.0f;
	return color;
}

float3 ComputeOverdrawColor( uint3 DTid )
{
	const int2 bufferPos = DTid.xy;

	const uint pixelID = bufferPos.y * c_pixelFrameSize.x + bufferPos.x;
	const uint blockCellItemOffset = mad( bufferPos.y >> 3, c_pixelFrameSize.x >> 3, bufferPos.x >> 3 ) * HLSL_CELL_ITEM_PER_BUCKET_MAX_COUNT;
	uint blockCellItemID = firstBlockCellItemIDs[pixelID];		
	uint step = 0;
	while ( blockCellItemID != 0 )
	{
		const uint blockCellItemAddress = blockCellItemOffset + blockCellItemID;
		blockCellItemID = blockCellItems[blockCellItemAddress].nextID;
		++step;
	}
	
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

	return color;
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
	const uint2 screenPos = uint2( DTid.x, c_pixelFrameSize.y - DTid.y - 1 );
#if 1
	const float3 color = Blend( DTid, GTid );
#else
	const float3 color = ComputeOverdrawColor( DTid );
#endif
	outputColors[screenPos] = float4( color, 1.0f );
}

#endif // #if BUFFER_WIDTH == 1
