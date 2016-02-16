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
		for ( uint blockCellItemID = firstBlockCellItemIDs[pixelID]; blockCellItemID != 0; blockCellItemID = blockCellItems[blockCellItemID].nextID )
		{
			const uint depth23_hitMask9 = blockCellItems[blockCellItemID].depth23_hitMask9;			
			const uint r8g8b8a8 = blockCellItems[blockCellItemID].r8g8b8a8;
			for ( uint hitShift = 0; hitShift < 9; ++hitShift )
			{
				const uint hitMask = 1 << hitShift;
				if ( (depth23_hitMask9 & hitMask) != 0 && depth23_hitMask9 < pixels[hitShift].depth23_hitMask9 )
				{
					pixels[hitShift].depth23_hitMask9 = depth23_hitMask9;
					pixels[hitShift].r8g8b8a8 = r8g8b8a8;
				}
			}
		}
	}

	g_pixelBuffers[groupID.y][groupID.x].pixels = pixels;
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GDTid : SV_GroupThreadID )
{
	const uint2 groupID = GDTid.xy + 1;
	const int2 bufferPos = DTid.xy;
	const int2 bufferBasePos = bufferPos - groupID;
	const uint2 screenPos = uint2( bufferPos.x, c_pixelFrameSize.y - bufferPos.y - 1 );
	
	// Sample this pixel
		
	SamplePixel( bufferBasePos, groupID );
	
	// Sample an external pixel at the boundary of the 8x8 grid

	if ( GDTid.y < 4 || GDTid.x == 0 )
	{		
		uint2 borderGroupID;
		switch ( GDTid.y )
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
	outputColors[screenPos] = float4( color, 1.0f );
}

#endif // #if BUFFER_WIDTH == 1

#if BUFFER_WIDTH == 3

struct PixelBuffer_s
{
	uint depth23_any9;
	uint r8g8b8a8;
};

static const uint s_depthMax = 0xFFFFFFFF;
static const uint pixelPerGroup = 64;
static const uint samplePerPixel = BUFFER_WIDTH * BUFFER_WIDTH;

float3 Blend( uint2 screenPos, uint2 bufferPos )
{
	PixelBuffer_s pixels[samplePerPixel];
	{
		for (uint sampleID = 0; sampleID < samplePerPixel; ++sampleID)
			pixels[sampleID].depth23_any9 = s_depthMax;
	}
	
	const uint pixelID = bufferPos.y * c_pixelFrameSize.x + bufferPos.x;

#if HLSL_DEBUG_PIXEL == 1
	uint itemID = 0;
#endif // #if HLSL_DEBUG_PIXEL == 1
	for ( uint blockCellItemID = firstBlockCellItemIDs[pixelID]; blockCellItemID != 0; blockCellItemID = blockCellItems[blockCellItemID].nextID )
	{
		const uint depth23_occupancy9 = blockCellItems[blockCellItemID].depth23_occupancy9;
		const uint r8g8b8a8 = blockCellItems[blockCellItemID].r8g8b8a8;
		for ( uint sampleID = 0; sampleID < samplePerPixel; ++sampleID )
		{
			if ( (depth23_occupancy9 & (1 << sampleID)) != 0 && depth23_occupancy9 < pixels[sampleID].depth23_any9 )
			{
				pixels[sampleID].depth23_any9 = depth23_occupancy9;
				pixels[sampleID].r8g8b8a8 = r8g8b8a8;
			}
		}
#if HLSL_DEBUG_PIXEL == 1
		if ( c_pixelDebug )
		{
			debugBuffer.cellItems[itemID] = blockCellItems[blockCellItemID];
			++itemID;
		}
#endif // #if HLSL_DEBUG_PIXEL == 1
	}

	uint3 rasterColorSum = uint3( 0, 0, 0 );
	uint rasterCount = 0;

	{
		for ( uint sampleID = 0; sampleID < samplePerPixel; ++sampleID )
		{
			if ( pixels[sampleID].depth23_any9 != s_depthMax )
			{
				rasterColorSum += UnpackRGBA( pixels[sampleID].r8g8b8a8 );
				rasterCount += 1.0f;
			}
		}
	}

	if ( rasterCount == 0 )
		return c_pixelBackColor;
	else
		// fixme: this is done to fill some holes between joint voxels that should'nt happen.
#if 1
		return rasterColorSum * (1.0f / (BUFFER_WIDTH * BUFFER_WIDTH * 255.0f ));
#else
		return rasterColorSum * rcp( rasterCount * 255.0f );
#endif
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint2 bufferPos = DTid.xy;
	const uint2 screenPos = uint2( bufferPos.x, c_pixelFrameSize.y - bufferPos.y - 1 );
	outputColors[screenPos] = float4( Blend( screenPos, bufferPos ), 1.0f );
}

#endif // #if BUFFER_WIDTH == 3