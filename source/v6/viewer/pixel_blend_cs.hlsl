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

struct PixelBuffer_s
{
	uint depth23_any9;
	uint r8g8b8a8;
};

static const uint s_depthMax = 0xFFFFFFFF;
static const uint pixelPerGroup = 64;
static const uint samplePerPixel = BUFFER_WIDTH * BUFFER_WIDTH;

uint3 UnpackRGBA( uint rgba )
{
	return uint3( (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF );
}

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
