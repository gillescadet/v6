#define HLSL
#include "trace_shared.h"

StructuredBuffer< BlockCellItem > blockCellItems	: REGISTER_SRV( HLSL_BLOCK_CELL_ITEM_SLOT );
Buffer< uint > cellItemCounters						: REGISTER_SRV( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT );

RWTexture2D< float4 > outputColors					: REGISTER_UAV( HLSL_COLOR_SLOT );
RWBuffer< uint > outputDisplacements				: REGISTER_UAV( HLSL_DISPLACEMENT_SLOT );

uint3 UnpackRGB_NONE( uint rgb_none )
{
	return uint3( (rgb_none >> 24) & 0xFF, (rgb_none >> 16) & 0xFF, (rgb_none >> 8) & 0xFF );
}

#define DEPTH_MAX 3.40282347E+38f

struct Pixel_s
{
	float depth;
	uint r8g8b8_none8;
	uint xdsp16_ydsp16;
};

struct PixelBuffer_s
{
	Pixel_s pixels[9];
};

groupshared PixelBuffer_s g_pixelBuffers[10][10];

#define UPDATE_OTHER_PIXEL( PIXELID, HITMASK9, DEPTH, R8G8B8_NONE8, XDSP16_YDSP16 ) \
	if ( (HITMASK9 & (1 << PIXELID)) != 0 && DEPTH < pixels[PIXELID].depth ) \
	{ \
		pixels[PIXELID].depth = DEPTH; \
		pixels[PIXELID].r8g8b8_none8 = R8G8B8_NONE8; \
		pixels[PIXELID].xdsp16_ydsp16 = XDSP16_YDSP16; \
	}

#define UPDATE_THIS_PIXEL( OTHER_X, OTHER_Y, OTHER_PIXEL_ID ) \
	{ \
		const uint2 otherGroupID = groupID + int2( OTHER_X, OTHER_Y ); \
		const float depth = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].depth; \
		const uint r8g8b8_none8 = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].r8g8b8_none8; \
		const uint xdsp16_ydsp16 = g_pixelBuffers[otherGroupID.y][otherGroupID.x].pixels[OTHER_PIXEL_ID].xdsp16_ydsp16; \
		if ( depth < this_depth ) \
		{ \
			this_depth = depth; \
			this_r8g8b8_none8 = r8g8b8_none8; \
			this_xdsp16_ydsp16 = xdsp16_ydsp16; \
		} \
	}

void SamplePixel( int2 bufferBasePos, uint2 groupID )
{
	Pixel_s pixels[9];

	for ( int hitShift = 0; hitShift < 9; ++hitShift )
	{
		pixels[hitShift].depth = DEPTH_MAX;
		pixels[hitShift].r8g8b8_none8 = 0;
		pixels[hitShift].xdsp16_ydsp16 = 0;
	}

	const int2 bufferPos = bufferBasePos + groupID;
	const int2 pixelCoords = int2( bufferPos.x - c_blendEye * c_blendFrameSize.x, bufferPos.y );
	if ( pixelCoords.x >= 0 && pixelCoords.x < (int)c_blendFrameSize.x && pixelCoords.y >= 0 && pixelCoords.y < (int)c_blendFrameSize.y )
	{
		const int cellItemCountPerPage = c_blendFrameSize.x * c_blendEyeCount * c_blendFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
		const uint pixelID = (mad( bufferPos.y >> 3, (c_blendFrameSize.x * c_blendEyeCount) >> 3, bufferPos.x >> 3 ) << 6) + mad( bufferPos.y & 7, 8, bufferPos.x & 7 );
		const uint blockCellItemCount = min( cellItemCounters[pixelID], HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT );
		for ( uint blockCellItemRank = 0; blockCellItemRank < blockCellItemCount; ++blockCellItemRank )
		{
			const uint blockCellItemPage = blockCellItemRank >> HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT;
			const uint blockCellItemRankInPage = blockCellItemRank & HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK;
			const uint blockCellItemID = blockCellItemPage * cellItemCountPerPage + HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * pixelID + blockCellItemRankInPage;
			
			const uint hitMask9 = ((blockCellItems[blockCellItemID].hitMask1_depth31 >> 23) & 0x100) | (blockCellItems[blockCellItemID].r8g8b8_hitMask8 & 0xFF);
			const float depth = asfloat( blockCellItems[blockCellItemID].hitMask1_depth31 & 0x7FFFFFFF );
			const uint r8g8b8_none8 = blockCellItems[blockCellItemID].r8g8b8_hitMask8;
			const uint xdsp16_ydsp16 = blockCellItems[blockCellItemID].xdsp16_ydsp16;

			UPDATE_OTHER_PIXEL( 0, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			UPDATE_OTHER_PIXEL( 1, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			UPDATE_OTHER_PIXEL( 2, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );

			UPDATE_OTHER_PIXEL( 3, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			UPDATE_OTHER_PIXEL( 4, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			UPDATE_OTHER_PIXEL( 5, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			
			UPDATE_OTHER_PIXEL( 6, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			UPDATE_OTHER_PIXEL( 7, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
			UPDATE_OTHER_PIXEL( 8, hitMask9, depth, r8g8b8_none8, xdsp16_ydsp16 );
		}
	}

	g_pixelBuffers[groupID.y][groupID.x].pixels = pixels;
}

void Blend( out float3 out_color, out uint out_displacement, uint2 bufferPos, uint3 GTid )
{
	const uint2 groupID = GTid.xy + 1;
	const int2 bufferBasePos = bufferPos - groupID;
	
	// Sample this pixel
		
	SamplePixel( bufferBasePos, groupID );
	
	// Sample one extra pixel at the boundary of the 8x8 grid

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
		
	GroupMemoryBarrierWithGroupSync();

	float this_depth = g_pixelBuffers[groupID.y][groupID.x].pixels[4].depth;
	uint this_r8g8b8_none8 = g_pixelBuffers[groupID.y][groupID.x].pixels[4].r8g8b8_none8;
	uint this_xdsp16_ydsp16 = g_pixelBuffers[groupID.y][groupID.x].pixels[4].xdsp16_ydsp16;

	UPDATE_THIS_PIXEL( -1, -1, 8 );
	UPDATE_THIS_PIXEL(  0, -1, 7 );
	UPDATE_THIS_PIXEL( +1, -1, 6 );

	UPDATE_THIS_PIXEL( -1,  0, 5 );
	UPDATE_THIS_PIXEL( +1,  0, 3 );
	
	UPDATE_THIS_PIXEL( -1, +1, 2 );
	UPDATE_THIS_PIXEL(  0, +1, 1 );
	UPDATE_THIS_PIXEL( +1, +1, 0 );
		
	out_color = UnpackRGB_NONE( this_r8g8b8_none8 ) / 255.0f;
	out_displacement = this_xdsp16_ydsp16;
}

float3 ComputeOverdrawColor( uint2 bufferPos )
{
	const int cellItemCountPerPage = c_blendFrameSize.x * c_blendFrameSize.y * HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT;
	const uint pixelID = (mad( bufferPos.y >> 3, (c_blendFrameSize.x * c_blendEyeCount) >> 3, bufferPos.x >> 3 ) << 6) + mad( bufferPos.y & 7, 8, bufferPos.x & 7 );
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
	const uint2 screenBufferPos = uint2( DTid.x, c_blendFrameSize.y - DTid.y - 1 );
	const uint2 stereoBuffPos = uint2( DTid.x + c_blendEye * c_blendFrameSize.x, DTid.y );

	float3 color;
	uint displacement = 0;

#if PIXEL_OVERDRAW == 1
	color = ComputeOverdrawColor( stereoBuffPos );
#elif 1
	Blend( color, displacement, stereoBuffPos, GTid );
#else
	const float3 color = WarmGPU( stereoBuffPos );
#endif

	const uint pixelID = mad( screenBufferPos.y, c_blendFrameSize.x, screenBufferPos.x );

	outputColors[screenBufferPos] = float4( color, 1.0f );
	outputDisplacements[pixelID] = displacement;
}
