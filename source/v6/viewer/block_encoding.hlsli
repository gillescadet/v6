struct EncodedBlock
{
	uint	blockPos;	
	uint	cellEndColors;
	uint	cellPresence[2];	
	uint	cellColorIndices[4];
};

uint4 UnpackRGBA( uint rgba )
{
	return uint4( (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF );
}

uint PackRGBA( uint3 rgba )
{
	return (rgba.r << 24) | (rgba.g << 16) | (rgba.b << 8);
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
	
	colors[2].r = (171 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (171 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (171 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 171 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 171 * colors[1].g) >> 8;
	colors[3].b = (85 * colors[0].b + 171 * colors[1].b) >> 8;

	// Output cell presence

	uint cellPresence[2];
	uint cellPosToID[64];

	cellPresence[0] = 0;
	cellPresence[1] = 0;

	{
		for ( uint cellPos = 0; cellPos < 64; ++cellPos )
			cellPosToID[cellPos] = (uint)-1;

		for ( uint cellID = 0; cellID < cellCount; ++cellID )
		{
			const uint cellPos = UnpackRGBA( cellRGBA[cellID] ).a;
			cellPresence[cellPos >> 5] |= 1 << (cellPos & 0x1F);
			cellPosToID[cellPos] = cellID;
		}
	}

	block.cellPresence[0] = cellPresence[0];
	block.cellPresence[1] = cellPresence[1];

	// Ouput cell colors

	uint cellColorIndices[4];
	cellColorIndices[0] = 0;
	cellColorIndices[1] = 0;
	cellColorIndices[2] = 0;
	cellColorIndices[3] = 0;
	
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

			cellColorIndices[cellRank >> 4] |= bestColorID << ((cellRank << 1) & 0x1F);
			++cellRank;
		}
	}

	block.cellColorIndices[0] = cellColorIndices[0];
	block.cellColorIndices[1] = cellColorIndices[1];
	block.cellColorIndices[2] = cellColorIndices[2];
	block.cellColorIndices[3] = cellColorIndices[3];

	return block;
}

void Block_DecodeColors( uint cellEndColors, out uint paletteColors[4] )
{
	// Decode min/max

	const uint color0 = (cellEndColors >> 0 ) & 0xFFFF;
	const uint color1 = (cellEndColors >> 16) & 0xFFFF;

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

	colors[2].r = (171 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (171 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (171 * colors[0].b + 85 * colors[1].b) >> 8;

	colors[3].r = (85 * colors[0].r + 171 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 171 * colors[1].g) >> 8;	
	colors[3].b = (85 * colors[0].b + 171 * colors[1].b) >> 8;

	paletteColors[0] = PackRGBA( colors[0] );
	paletteColors[1] = PackRGBA( colors[1] );
	paletteColors[2] = PackRGBA( colors[2] );
	paletteColors[3] = PackRGBA( colors[3] );
}

/* 

{
const uint dataSizePerBucket[] = { 4, 4, 4 , 5 , 7  };
const uint dataSize = dataSizePerBucket[GRID_CELL_BUCKET];
const uint firstDataOffset = block_dataOffset( GRID_CELL_BUCKET );	
const uint blockDataID = firstDataOffset + blockID * dataSize;

const uint endPointColors = blockData[blockDataID+0];
uint presenceBits = blockData[blockDataID+1];

uint cellPosPacked[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint cellPosOffset = 0;
uint cellCount = 0;

for (;;)
{
if ( presenceBits == 0 && cellPosOffset == 0 )
{
presenceBits = blockData[blockDataID+2];
cellPosOffset = 32;
}

int cellPos = firstbitlow( presenceBits );
if ( cellPos == -1 )
break;

presenceBits -= (1 << cellPos);
cellPos += cellPosOffset;

const uint cellPosBucket = cellCount >> 2;
const uint cellPosShift = (cellCount & 3) << 3;
cellPosPacked[cellPosBucket] |= cellPos << cellPosShift;
++cellCount;
}

uint cellBaseID = 0;  
InterlockedAdd( trace_cellCount, cellCount, cellBaseID );
cellBaseID *= 2;

uint paletteColors[4];
Block_DecodeColors( endPointColors, paletteColors );

const uint cellColorBucketCount = (cellCount + 15) >> 4;
uint cellRank = 0;
for ( uint cellColorBucket = 0; cellColorBucket < cellColorBucketCount; ++cellColorBucket )
{
uint cellColorIndices = blockData[blockDataID+3+cellColorBucket];
for ( uint cellColorKey = 0; cellColorKey < 16 && cellRank < cellCount; ++cellColorKey, ++cellRank, cellColorIndices >>= 2 )
{					
const uint cellPosBucket = cellRank >> 2;
const uint cellPos = cellPosPacked[cellPosBucket] & 0xFF;
cellPosPacked[cellPosBucket] >>= 8;

const uint colorID = cellColorIndices & 3;				
const uint cellRGB_none = paletteColors[colorID];

// optimization: find a way to write the block pos only once
traceCells[cellBaseID + cellRank*2 + 0] = packedBlockPos;
traceCells[cellBaseID + cellRank*2 + 1] = cellRGB_none | cellPos;
}
}

*/