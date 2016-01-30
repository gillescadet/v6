#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

struct BlockCell
{	
	float3	posWS;
	float	halfCellSize;
	uint	color;
	uint	mip;
	uint	occupancy;
};

struct EncodedBlock
{
	uint	blockPos;
	uint	cellEndColors;
	uint	cellColorIndices[4];
	uint	cellPresence[2];
};

struct DecodedBlock
{
	uint	blockPos;
	uint	cellRGBA[64];
	uint	cellCount;
};

bool PackedColor_Unpack( uint packedID, uint packedOffset, out BlockCell o )
{
	o.posWS = float3( 0.0f, 0.0f, 0.0f );
	o.halfCellSize = 0.0f;
	o.color = 0;
	o.mip = 0;
	o.occupancy = 0;

	const uint blockID = packedID >> GRID_CELL_SHIFT;	
	const uint packedCount = 1 + GRID_CELL_COUNT * HLSL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint packedRank = packedBaseID + 1 + (packedID & GRID_CELL_MASK) * HLSL_COUNT;
	const uint packedColor = blockColors[packedRank + 0];

	if ( packedColor == HLSL_GRID_BLOCK_CELL_EMPTY )
	{
		return false;
	}
	else
	{
		o.color = packedColor | 0xFF;

		const uint packedPos = blockColors[packedBaseID];
		o.mip = packedPos >> 28;
		o.occupancy = blockColors[packedRank + 1];

		const uint blockPos = packedPos & 0x0FFFFFFF;
		const uint cellPos = packedColor & 0x3F;
		const uint x = (((blockPos >> 0						 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
		const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
		const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);
		const int4 cellCoords = int4( x, y, z, 0 );	
		const float gridScale = c_blockGridScales[o.mip].x;
		o.halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
		o.posWS = mad( cellCoords.xyz, o.halfCellSize * 2.0, -gridScale + o.halfCellSize ) + c_blockCenter;

		return true;
	}	
}

uint4 UnpackRGBA( uint rgba )
{
	return uint4( (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF );
}

uint PackRGBA( uint4 rgba )
{
	return (rgba.r << 24) | (rgba.g << 16) | (rgba.b << 8) | rgba.a;
}

EncodedBlock Block_Encode( uint blockPos, uint cellRGBA[64], uint cellCount )
{
	EncodedBlock block;

	// Output blockPos
	
	block.blockPos = blockPos;

	// Find the min/max colors
	
	uint3 minColor = uint3( 255, 255, 255 );
	uint3 maxColor = uint3(   0,   0,   0 );

	{
		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint4 color = UnpackRGBA( cellRGBA[cellID] );

			minColor = min( minColor, color.rgb );
			maxColor = max( maxColor, color.rgb );
		}	

		const uint3 extentColor = (maxColor - minColor) >> 4;

		minColor.r = minColor.r + extentColor.r < 255 ? (minColor.r + extentColor.r) : 255;
		minColor.g = minColor.g + extentColor.g < 255 ? (minColor.g + extentColor.g) : 255;
		minColor.b = minColor.b + extentColor.b < 255 ? (minColor.b + extentColor.b) : 255;

		maxColor.r = maxColor.r > extentColor.r ? (maxColor.r - extentColor.r) : 0;
		maxColor.g = maxColor.g > extentColor.g ? (maxColor.g - extentColor.g) : 0;
		maxColor.b = maxColor.b > extentColor.b ? (maxColor.b - extentColor.b) : 0;
	}

	// Output colors

	const uint color0 = ((maxColor.r >> 3) << 11) | ((maxColor.g >> 2) << 5) | (maxColor.b >> 3);
	const uint color1 = ((minColor.r >> 3) << 11) | ((minColor.g >> 2) << 5) | (minColor.b >> 3);
	block.cellEndColors = (color1 << 16) | color0;

	// Make palette

	uint3 colors[4];

	colors[0].r = (maxColor.r & 0xF8) | (maxColor.r >> 5);
	colors[0].g = (maxColor.g & 0xFC) | (maxColor.g >> 6);
	colors[0].b = (maxColor.b & 0xF8) | (maxColor.b >> 5);

	colors[1].r = (minColor.r & 0xF8) | (minColor.r >> 5);
	colors[1].g = (minColor.g & 0xFC) | (minColor.g >> 6);
	colors[1].b = (minColor.b & 0xF8) | (minColor.b >> 5);
	
	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

	// Output cell presence

	block.cellPresence[0] = 0;
	block.cellPresence[1] = 0;

	uint cellPosToID[64];

	{
		for ( uint cellPos = 0; cellPos < 64; ++cellPos )
			cellPosToID[cellPos] = (uint)-1;

		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint cellPos = UnpackRGBA( cellRGBA[cellID] ).a;
			block.cellPresence[cellPos >> 5] |= 1 << (cellPos & 0x1F);
			cellPosToID[cellPos] = cellID;
		}
	}

	// Ouput cell colors

	block.cellColorIndices[0] = 0;
	block.cellColorIndices[1] = 0;
	block.cellColorIndices[2] = 0;
	block.cellColorIndices[3] = 0;
	
	{
		uint cellRank = 0;
		for ( uint cellPos = 0; cellPos < 64; ++cellPos )
		{
			if ( cellPosToID[cellPos] == (uint)-1 )
				continue;

			const uint cellID = cellPosToID[cellPos];
			const uint4 color = UnpackRGBA( cellRGBA[cellID] );

			uint bestColorID = 0;
			uint bestError = 0xFFFFFFFF;
			
			for ( uint colorID = 0; colorID < 4; ++colorID )
			{
				const int dR = color.r - colors[colorID].r;
				const int dG = color.g - colors[colorID].g;
				const int dB = color.b - colors[colorID].b;

				const uint error = dR * dR + dG * dG + dB * dB;
				if ( error < bestError )
				{
					bestError = error;
					bestColorID = colorID;
				}
			}

			block.cellColorIndices[cellRank >> 4] |= bestColorID << ((cellRank << 1) & 0x1F);
			++cellRank;
		}
	}

	return block;
}

DecodedBlock Block_Decode( EncodedBlock encodedBlock )
{
	DecodedBlock decodedBlock;

	// Decode blockPos

	decodedBlock.blockPos = encodedBlock.blockPos;

	// Decode min/max

	const uint color0 = (encodedBlock.cellEndColors >> 0 ) & 0xFFFF;
	const uint color1 = (encodedBlock.cellEndColors >> 16) & 0xFFFF;

	uint3 maxColor;
	maxColor.r = ((color0 >> 11) & 0x1F) << 3;
	maxColor.g = ((color0 >>  5) & 0x3F) << 2;
	maxColor.b = ((color0 >>  0) & 0x1F) << 3;
	
	uint3 minColor;
	minColor.r = ((color1 >> 11) & 0x1F) << 3;
	minColor.g = ((color1 >>  5) & 0x3F) << 2;
	minColor.b = ((color1 >>  0) & 0x1F) << 3;

	// Make palette

	uint3 colors[4];
	
	colors[0].r = maxColor.r | (maxColor.r >> 5);
	colors[0].g = maxColor.g | (maxColor.g >> 6);
	colors[0].b = maxColor.b | (maxColor.b >> 5);

	colors[1].r = minColor.r | (minColor.r >> 5);
	colors[1].g = minColor.g | (minColor.g >> 6);
	colors[1].b = minColor.b | (minColor.b >> 5);
	
	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;	
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

	// Decode bits

	decodedBlock.cellCount = 0;
	
	for ( uint cellPos = 0; cellPos < 64; ++cellPos )
	{
		if ( (encodedBlock.cellPresence[cellPos >> 5] & (1 << (cellPos & 0x1F))) == 0 )
			continue;

		const uint cellRank = decodedBlock.cellCount;
		const uint colorID = (encodedBlock.cellColorIndices[cellRank >> 4] >> ((cellRank << 1) & 0x1F)) & 3;
		decodedBlock.cellRGBA[cellRank] = PackRGBA( uint4( colors[colorID], cellPos ) );
		++decodedBlock.cellCount;
	}

	return decodedBlock;
}