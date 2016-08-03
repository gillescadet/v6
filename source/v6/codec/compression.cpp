/*V6*/

#include <v6/core/common.h>

#include <v6/codec/compression.h>
#include <v6/core/color.h>
#include <v6/core/mat3x3.h>
#include <v6/core/math.h>
#include <v6/core/optimization.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>

BEGIN_V6_NAMESPACE

static void EndColor_F32ToU8( Color_s* endColorU8, const Vec3* endColorF32 )
{
	endColorU8->r = (u8)Clamp( endColorF32->x * 255.0f, 0.0f, 255.0f );
	endColorU8->g = (u8)Clamp( endColorF32->y * 255.0f, 0.0f, 255.0f );
	endColorU8->b = (u8)Clamp( endColorF32->z * 255.0f, 0.0f, 255.0f );
	endColorU8->a = 0;

	endColorU8->r = (endColorU8->r & ~7) | (endColorU8->r >> 5);
	endColorU8->g = (endColorU8->g & ~3) | (endColorU8->g >> 6);
	endColorU8->b = (endColorU8->b & ~7) | (endColorU8->b >> 5);
}

static void EndColor_U8ToF32( Vec3* endColorF32, const Color_s* endColorU8 )
{
	endColorF32->x = endColorU8->r / 255.0f;
	endColorF32->y = endColorU8->g / 255.0f;
	endColorF32->z = endColorU8->b / 255.0f;
}

static u32 Block_Encode_Build( EncodedBlockEx_s* encodedBlock, u32 cellRGBA[64], u32 cellCount, Color_s endColor0, Color_s endColor1 )
{
	// Output colors

	{
		u32 color0 = ((endColor0.r >> 3) << 11) | ((endColor0.g >> 2) << 5) | (endColor0.b >> 3);
		u32 color1 = ((endColor1.r >> 3) << 11) | ((endColor1.g >> 2) << 5) | (endColor1.b >> 3);

		if ( color0 < color1 )
		{
			Swap( color0, color1 );
			Swap( endColor0, endColor1 );
		}

		encodedBlock->cellEndColors = (color1 << 16) | color0;
	}

	// Make palette

	Color_s colors[4];

	colors[0].r = (endColor0.r & 0xF8) | (endColor0.r >> 5);
	colors[0].g = (endColor0.g & 0xFC) | (endColor0.g >> 6);
	colors[0].b = (endColor0.b & 0xF8) | (endColor0.b >> 5);
	
	colors[1].r = (endColor1.r & 0xF8) | (endColor1.r >> 5);
	colors[1].g = (endColor1.g & 0xFC) | (endColor1.g >> 6);
	colors[1].b = (endColor1.b & 0xF8) | (endColor1.b >> 5);

	colors[2].r = (170 * colors[0].r + 85 * colors[1].r) >> 8;
	colors[2].g = (170 * colors[0].g + 85 * colors[1].g) >> 8;
	colors[2].b = (170 * colors[0].b + 85 * colors[1].b) >> 8;
	
	colors[3].r = (85 * colors[0].r + 170 * colors[1].r) >> 8;
	colors[3].g = (85 * colors[0].g + 170 * colors[1].g) >> 8;
	colors[3].b = (85 * colors[0].b + 170 * colors[1].b) >> 8;

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

	u32 sumError = 0;

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
				const int dR = colorR - colors[colorID].r;
				const int dG = colorG - colors[colorID].g;
				const int dB = colorB - colors[colorID].b;

				const u32 error = dR * dR + dG * dG + dB * dB;
				if ( error < bestError )
				{
					bestError = error;
					bestColorID = colorID;
				}
			}

			encodedBlock->cellColorIndices[cellRank >> 5] |= (u64)bestColorID << ((cellRank << 1) & 0x3F);
			++cellRank;

			sumError += bestError;
		}
	}

	return sumError;
}

template < int RADIUS >
static u32 Block_Encode_Refine( EncodedBlockEx_s* encodedBlock, u32 cellRGBA[64], u32 cellCount, const Vec3* endColor0, const Vec3* endColor1 )
{
	// Converts to 8 bits
	
	Color_s min;
	min.r = (u8)Clamp( endColor0->x * 255.0f, 0.0f, 255.0f );
	min.g = (u8)Clamp( endColor0->y * 255.0f, 0.0f, 255.0f );
	min.b = (u8)Clamp( endColor0->z * 255.0f, 0.0f, 255.0f );

	Color_s max;
	max.r = (u8)Clamp( endColor1->x * 255.0f, 0.0f, 255.0f );
	max.g = (u8)Clamp( endColor1->y * 255.0f, 0.0f, 255.0f );
	max.b = (u8)Clamp( endColor1->z * 255.0f, 0.0f, 255.0f );

	Vec3u minHighBits;
	minHighBits.x = min.r >> 5;
	minHighBits.y = min.g >> 6;
	minHighBits.z = min.b >> 5;

	Vec3u maxHighBits;
	maxHighBits.x = max.r >> 5;
	maxHighBits.y = max.g >> 6;
	maxHighBits.z = max.b >> 5;

	min.r >>= 3;
	min.g >>= 2;
	min.b >>= 3;

	max.r >>= 3;
	max.g >>= 2;
	max.b >>= 3;
	
	Vec3u startMin, endMin;
	startMin.x = Max( 0, (int)min.r - RADIUS );
	startMin.y = Max( 0, (int)min.g - RADIUS );
	startMin.z = Max( 0, (int)min.b - RADIUS );
	endMin.x = Min( 31, (int)min.r + RADIUS );
	endMin.y = Min( 63, (int)min.g + RADIUS );
	endMin.z = Min( 31, (int)min.b + RADIUS );

	Vec3u startMax, endMax;
	startMax.x = Max( 0, (int)max.r - RADIUS );
	startMax.y = Max( 0, (int)max.g - RADIUS );
	startMax.z = Max( 0, (int)max.b - RADIUS );
	endMax.x = Min( 31, (int)max.r + RADIUS );
	endMax.y = Min( 63, (int)max.g + RADIUS );
	endMax.z = Min( 31, (int)max.b + RADIUS );

	u32 minError = INT_MAX;

	for ( min.r = startMin.x; min.r <= endMin.x; ++min.r )
	{
		for ( min.g = startMin.y; min.g <= endMin.y; ++min.g )
		{
			for ( min.b = startMin.z; min.b <= endMin.z; ++min.b )
			{
				for ( max.r = startMax.x; max.r <= endMax.x; ++max.r )
				{
					for ( max.g = startMax.y; max.g <= endMax.y; ++max.g )
					{
						for ( max.b = startMax.z; max.b <= endMax.z; ++max.b )
						{
							Color_s discretEndColor0 = min;
							discretEndColor0.r = (min.r << 3) | minHighBits.x;
							discretEndColor0.g = (min.g << 2) | minHighBits.y;
							discretEndColor0.b = (min.b << 3) | minHighBits.z;

							Color_s discretEndColor1 = max;
							discretEndColor1.r = (max.r << 3) | maxHighBits.x;
							discretEndColor1.g = (max.g << 2) | maxHighBits.y;
							discretEndColor1.b = (max.b << 3) | maxHighBits.z;

							EncodedBlockEx_s curBlock;
							const u32 error = Block_Encode_Build( &curBlock, cellRGBA, cellCount, discretEndColor0, discretEndColor1 );

							if ( error < minError )
							{
								minError = error;
								*encodedBlock = curBlock;
							}
						}
					}
				}
			}
		}
	}

	return minError;
}

u32 Block_Encode_Optimize( EncodedBlockEx_s* encodedBlock, u32 cellRGBA[64], u32 cellCount )
{
	// Compute centred colors

	Vec3 colorCenter = Vec3_Zero();
	Vec3 centredColors[64];

	{
		Vec3 colors[64];
		for ( u32 cellID = 0; cellID < cellCount; ++cellID )
		{
			if ( cellRGBA[cellID] == 0xFFFFFFFF )
			{
				cellCount = cellID;
				break;
			}

			const u32 pixel = cellRGBA[cellID];
			colors[cellID].x = ((pixel >> 24) & 0xFF) / 255.0f;
			colors[cellID].y = ((pixel >> 16) & 0xFF) / 255.0f;
			colors[cellID].z = ((pixel >>  8) & 0xFF) / 255.0f;
			colorCenter += colors[cellID];
		}

		V6_ASSERT( cellCount > 0 );
		colorCenter *= 1.0f / cellCount;
	
		for ( u32 colorID = 0; colorID < cellCount; ++colorID )
			centredColors[colorID] = colors[colorID] - colorCenter;
	}

	// Compute best line passing by centred colors

	Vec3 colorDir;

	{
		Mat3x3 covariance;
		Optimization_FindBestFittingLine3DPrecentred( &colorDir, &covariance, centredColors, cellCount );

		if ( covariance.m_row0.x < FLT_EPSILON && covariance.m_row1.y < FLT_EPSILON && covariance.m_row2.z < FLT_EPSILON )
			return Block_Encode_Refine< 1 >( encodedBlock, cellRGBA, cellCount, &colorCenter, &colorCenter );
	}

	Vec3 bestEndColor0;
	Vec3 bestEndColor1;
	float minSumDistanceSQ = FLT_MAX;

	{
		// Project centred colors on line to find bounds

		float tMin = FLT_MAX;
		float tMax = -FLT_MAX;
		for ( u32 colorID = 0; colorID < cellCount; ++colorID )
		{
			const float ts = Dot( centredColors[colorID], colorDir );
			tMin = Min( tMin, ts );
			tMax = Max( tMax, ts );
		}

		// Try multiple segments

		const float tLen = (tMax - tMin);
		const float tStep = tLen * 0.0125f;

		static const float colorEps = 3.0f * (2.0f / 255.f) * (2.0f / 255.0f);

		for ( int step0 = -4; step0 <= 4; ++step0 )
		{
			const float t0 = tMin + step0 * tStep;
			for ( int step1 = -4; step1 <= 4; ++step1 )
			{
				const float t1 = tMax + step1 * tStep;
				const float tDiff = t1 - t0;
				if ( tDiff < colorEps )
					continue;

				Vec3 endColor0 = colorCenter + t0 * colorDir;
				Vec3 endColor1 = colorCenter + t1 * colorDir;
				if ( endColor0.Max() < 0.0f || endColor0.Min() > 1.0f || endColor1.Max() < 0.0f || endColor1.Min() > 1.0f )
					continue;

				// Snap on the end color grid

				{
					Color_s encodedColor0;
					Color_s encodedColor1;
					EndColor_F32ToU8( &encodedColor0, &endColor0 );
					EndColor_F32ToU8( &encodedColor1, &endColor1 );
					EndColor_U8ToF32( &endColor0, &encodedColor0 );
					EndColor_U8ToF32( &endColor1, &encodedColor1 );
				}

				// Project centred colors on segment to find decoded colors

				float segmentSumDistanceSQ = 0.0f;

				Vec3 binColors[4];
				binColors[0] = endColor0 - colorCenter;
				binColors[3] = endColor1 - colorCenter;
				binColors[1] = Lerp( binColors[0], binColors[3], 1.0f / 3.0f );
				binColors[2] = Lerp( binColors[0], binColors[3], 2.0f / 3.0f );
				
				for ( u32 colorID = 0; colorID < cellCount; ++colorID )
				{
					float bestDistanceSQ = FLT_MAX;
					for ( u32 binID = 0; binID < 4; ++binID )
					{
						const float distanceSQ = (centredColors[colorID] - binColors[binID]).LengthSq();
						bestDistanceSQ = Min( bestDistanceSQ, distanceSQ );
					}
					
					segmentSumDistanceSQ += bestDistanceSQ;
				}

				// Keep the best segment

				if ( segmentSumDistanceSQ < minSumDistanceSQ )
				{
					minSumDistanceSQ = segmentSumDistanceSQ;
					bestEndColor0 = endColor0;
					bestEndColor1 = endColor1;
				}
			}
		}
	}

	if ( minSumDistanceSQ == FLT_MAX )
		return Block_Encode_Refine< 1 >( encodedBlock, cellRGBA, cellCount, &colorCenter, &colorCenter );

	return Block_Encode_Refine< 1 >( encodedBlock, cellRGBA, cellCount, &bestEndColor0, &bestEndColor1 );
}

u32 Block_Encode_BoundingBox( EncodedBlockEx_s* encodedBlock, u32 cellRGBA[64], u32 cellCount )
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
		u32 color0 = ((maxColorR >> 3) << 11) | ((maxColorG >> 2) << 5) | (maxColorB >> 3);
		u32 color1 = ((minColorR >> 3) << 11) | ((minColorG >> 2) << 5) | (minColorB >> 3);

		if ( color0 < color1 )
			Swap( color0, color1 );

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

	u32 sumError = 0;

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

			sumError += bestError;
		}
	}

	return sumError;
}

void Block_Decode( u32 cellRGBA[64], u32* cellCount, const EncodedBlockEx_s* encodedBlock )
{
	// Decode min/max

	const u32 color0 = (encodedBlock->cellEndColors >>  0) & 0xFFFF;
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

	for ( u32 cellID = 0; cellID < 64; ++cellID )
	{
		if ( (encodedBlock->cellPresence & (1ll << cellID)) == 0 )
			continue;

		const u32 cellRank = *cellCount;
		const u32 colorID = (encodedBlock->cellColorIndices[cellRank >> 5] >> ((cellRank << 1) & 0x3F)) & 3;
		cellRGBA[cellRank] = (rs[colorID] << 24) | (gs[colorID] << 16) | (bs[colorID] << 8) | cellID;
		++(*cellCount);
	}
}

END_V6_NAMESPACE
