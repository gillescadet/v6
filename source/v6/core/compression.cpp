/*V6*/
#include <v6/core/common.h>

#include <v6/core/compression.h>
#include <v6/core/math.h>

BEGIN_V6_NAMESPACE

void Block_Encode( EncodedBlockEx_s* encodedBlock, u32 cellRGBA[64], u32 cellCount )
{
	// Ensure a correct cell count

	for ( u32 cellID = 0; cellID < cellCount; ++cellID )
	{
		if ( cellRGBA[cellID] == 0xFFFFFFFF )
		{
			cellCount = cellID;
			break;
		}
	}

	// Find the min/max colors

	u32 minColorR = 255;
	u32 minColorG = 255;
	u32 minColorB = 255;
	u32 maxColorR = 0;
	u32 maxColorG = 0;
	u32 maxColorB = 0;

	{
		for ( u32 cellID = 0; cellID < cellCount; ++cellID )
		{
			const u32 colorR = (cellRGBA[cellID] >> 24) & 0xFF;
			const u32 colorG = (cellRGBA[cellID] >> 16) & 0xFF;
			const u32 colorB = (cellRGBA[cellID] >>  8) & 0xFF;

			minColorR = Min( minColorR, colorR );
			minColorG = Min( minColorG, colorG );
			minColorB = Min( minColorB, colorB );
			maxColorR = Max( maxColorR, colorR );
			maxColorG = Max( maxColorG, colorG );
			maxColorB = Max( maxColorB, colorB );
		}	

		const u32 extentColorR = (maxColorR - minColorR) >> 4;
		const u32 extentColorG = (maxColorG - minColorG) >> 4;
		const u32 extentColorB = (maxColorB - minColorB) >> 4;

		minColorR = minColorR + ((extentColorR < 255) ? (minColorR + extentColorR) : 255);
		minColorG = minColorG + ((extentColorG < 255) ? (minColorG + extentColorG) : 255);
		minColorB = minColorB + ((extentColorB < 255) ? (minColorB + extentColorB) : 255);

		maxColorR = (maxColorR > extentColorR) ? (maxColorR - extentColorR) : 0;
		maxColorG = (maxColorG > extentColorG) ? (maxColorG - extentColorG) : 0;
		maxColorB = (maxColorB > extentColorB) ? (maxColorB - extentColorB) : 0;
	}

	// Output colors

	{
		const u32 color0 = ((maxColorR >> 3) << 11) | ((maxColorG >> 2) << 5) | (maxColorB >> 3);
		const u32 color1 = ((minColorR >> 3) << 11) | ((minColorG >> 2) << 5) | (minColorB >> 3);
		encodedBlock->cellEndColors = (color1 << 16) | color0;
	}

	// Make palette

	u32 rs[4];
	u32 gs[4];
	u32 bs[4];

	rs[0] = (maxColorR & 0xF8) | (maxColorR >> 5);
	gs[0] = (maxColorG & 0xFC) | (maxColorG >> 6);
	bs[0] = (maxColorB & 0xF8) | (maxColorB >> 5);

	rs[1] = (minColorR & 0xF8) | (minColorR >> 5);
	gs[1] = (minColorG & 0xFC) | (minColorG >> 6);
	bs[1] = (minColorB & 0xF8) | (minColorB >> 5);

	rs[2] = (170 * rs[0] + 85 * rs[1]) >> 8;
	gs[2] = (170 * gs[0] + 85 * gs[1]) >> 8;
	bs[2] = (170 * bs[0] + 85 * bs[1]) >> 8;

	rs[3] = (85 * rs[0] + 170 * rs[1]) >> 8;
	gs[3] = (85 * gs[0] + 170 * gs[1]) >> 8;
	bs[3] = (85 * bs[0] + 170 * bs[1]) >> 8;

	// Output cell presence
	
	encodedBlock->cellPresence = 0;

	u32 cellPosToID[64];

	{
		for ( u32 cellPos = 0; cellPos < 64; ++cellPos )
			cellPosToID[cellPos] = (u32)-1;

		for ( u32 cellID = 0; cellID < cellCount; ++cellID )
		{
			const u32 cellPos = cellRGBA[cellID] & 0x3F;
			encodedBlock->cellPresence |= 1ll << cellPos;
			cellPosToID[cellPos] = cellID;
		}
	}

	// Ouput cell colors

	encodedBlock->cellColorIndices[0] = 0;
	encodedBlock->cellColorIndices[1] = 0;

	{
		u32 cellRank = 0;
		for ( u32 cellPos = 0; cellPos < 64; ++cellPos )
		{
			if ( cellPosToID[cellPos] == (u32)-1 )
				continue;

			const u32 cellID = cellPosToID[cellPos];
			const u32 colorR = (cellRGBA[cellID] >> 24) & 0xFF;
			const u32 colorG = (cellRGBA[cellID] >> 16) & 0xFF;
			const u32 colorB = (cellRGBA[cellID] >>  8) & 0xFF;

			u32 bestColorID = 0;
			u32 bestError = 0xFFFFFFFF;

			for ( u32 colorID = 0; colorID < 4; ++colorID )
			{
				const int dR = colorR - rs[colorID];
				const int dG = colorG - gs[colorID];
				const int dB = colorB - bs[colorID];

				const u32 error = dR * dR + dG * dG + dB * dB;
				if ( error < bestError )
				{
					bestError = error;
					bestColorID = colorID;
				}
			}

			encodedBlock->cellColorIndices[cellRank >> 5] |= (u64)bestColorID << ((cellRank << 1) & 0x3F);
			++cellRank;
		}
	}
}

void Block_Decode( u32 cellRGBA[64], u32* cellCount, const EncodedBlockEx_s* encodedBlock )
{
	// Decode min/max

	const u32 color0 = (encodedBlock->cellEndColors >> 0 ) & 0xFFFF;
	const u32 color1 = (encodedBlock->cellEndColors >> 16) & 0xFFFF;

	const u32 maxColorR = ((color0 >> 11) & 0x1F) << 3;
	const u32 maxColorG = ((color0 >>  5) & 0x3F) << 2;
	const u32 maxColorB = ((color0 >>  0) & 0x1F) << 3;

	const u32 minColorR = ((color1 >> 11) & 0x1F) << 3;
	const u32 minColorG = ((color1 >>  5) & 0x3F) << 2;
	const u32 minColorB = ((color1 >>  0) & 0x1F) << 3;

	// Make palette

	u32 rs[4];
	u32 gs[4];
	u32 bs[4];

	rs[0] = maxColorR | (maxColorR >> 5);
	gs[0] = maxColorG | (maxColorG >> 6);
	bs[0] = maxColorB | (maxColorB >> 5);

	rs[1] = minColorR | (minColorR >> 5);
	gs[1] = minColorG | (minColorG >> 6);
	bs[1] = minColorB | (minColorB >> 5);

	rs[2] = (170 * rs[0] + 85 * rs[1]) >> 8;
	gs[2] = (170 * gs[0] + 85 * gs[1]) >> 8;
	bs[2] = (170 * bs[0] + 85 * bs[1]) >> 8;

	rs[3] = (85 * rs[0] + 170 * rs[1]) >> 8;
	gs[3] = (85 * gs[0] + 170 * gs[1]) >> 8;
	bs[3] = (85 * bs[0] + 170 * bs[1]) >> 8;

	// Decode bits

	*cellCount = 0;

	for ( u32 cellPos = 0; cellPos < 64; ++cellPos )
	{
		if ( (encodedBlock->cellPresence & (1ll << cellPos)) == 0 )
			continue;

		const u32 cellRank = *cellCount;
		const u32 colorID = (encodedBlock->cellColorIndices[cellRank >> 5] >> ((cellRank << 1) & 0x3F)) & 3;
		cellRGBA[cellRank] = (rs[colorID] << 24) | (gs[colorID] << 16) | (bs[colorID] << 8) | cellPos;
		++(*cellCount);
	}
}

END_V6_NAMESPACE
