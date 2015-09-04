#include "grid_fill.hlsli"

static const float3 lookAts[6] = 
{
	float3( 1.0f,  0.0f,  0.0f ),
	float3( -1.0f , 0.0f, 0.0f ),
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f, -1.0f,  0.0f ), 
	float3( 0.0f,  0.0f,  1.0f ), 
	float3( 0.0f,  0.0f, -1.0f )    
};

static const float3 ups[6] =
{
	float3( 0.0f,  1.0f,  0.0f ),
	float3(  0.0f , 1.0f, 0.0f ),
	float3( 0.0f,  0.0f, -1.0f ),
	float3( 0.0f,  0.0f,  1.0f ),
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f,  1.0f,  0.0f )
};

void SetCoverage( inout uint2 coverage, uint i, uint j, uint k )
{
	const uint bit = k * (HLSL_GRID_BLOCK_WIDTH * HLSL_GRID_BLOCK_WIDTH) + j * HLSL_GRID_BLOCK_WIDTH + i;
	const uint bucket = bit >> 5;
	const uint mask = (1 << (bit & 31));
	coverage[bucket] |= mask;
}

uint2 ComputeCoverage( float3 pixelRelativePos, float pixelRadius  )
{
	const float step = 0.25;
	const float halfStep = 0.125;
	const float pos[4] = { step * 0.0f + halfStep, step * 1.0f + halfStep, step * 2.0f + halfStep, step * 3.0f + halfStep };

	uint2 coverage = uint2( 0, 0 );

	[unroll]
	for ( uint k = 0; k < 4; ++k )
	{
		[unroll]
		for ( uint j = 0; j < 4; ++j )
		{
			[unroll]
			for ( uint i = 0; i < 4; ++i )
			{
				if ( all( abs( float3( pos[i], pos[j], pos[k] ) - pixelRelativePos ) < pixelRadius ) )
					SetCoverage( coverage, i, j, k );
			}
		}
	}

	return coverage;
}


void GetBlockIDAndCellPos( uint3 gridCoords, out uint blockID, out uint cellPos )
{
	const uint3 gridBlockCoords = gridCoords >> HLSL_GRID_BLOCK_SHIFT;
	const uint gridBlockPos = (gridBlockCoords.z << HLSL_GRID_MACRO_2XSHIFT) | (gridBlockCoords.y << HLSL_GRID_MACRO_SHIFT) | gridBlockCoords.x;

	const uint3 gridCellCoords = gridCoords & HLSL_GRID_BLOCK_MASK;
	cellPos = (gridCellCoords.z << HLSL_GRID_BLOCK_2XSHIFT) | (gridCellCoords.y << HLSL_GRID_BLOCK_SHIFT) | gridCellCoords.x;

	blockID = gridBlockIDs[gridBlockPos];
	if ( blockID == HLSL_GRID_BLOCK_INVALID )
	{
		InterlockedCompareExchange( gridBlockIDs[gridBlockPos], HLSL_GRID_BLOCK_INVALID, HLSL_GRID_BLOCK_SETTING, blockID );
		if ( blockID == HLSL_GRID_BLOCK_INVALID )
		{
			InterlockedAdd( gridIndirectArgs_blockCount, 1, blockID );

			uint prevBlockID;
			InterlockedExchange( gridBlockIDs[gridBlockPos], blockID, prevBlockID );

			const uint blockCount = blockID+1;
			InterlockedMax( gridIndirectArgs_packThreadGroupCount, (blockCount + HLSL_GRID_THREAD_GROUP_SIZE - 1) / HLSL_GRID_THREAD_GROUP_SIZE );		
			
			gridBlockPositions[blockID] = gridBlockPos;
		}
		else if ( blockID == HLSL_GRID_BLOCK_SETTING )
		{
			do
			{
				InterlockedCompareExchange( gridBlockIDs[gridBlockPos], HLSL_GRID_BLOCK_INVALID, HLSL_GRID_BLOCK_SETTING, blockID );
			} while ( blockID == HLSL_GRID_BLOCK_SETTING );
#if HLSL_DEBUG_FILL
			InterlockedAdd( gridIndirectArgs_waitCount0, 1 );
#endif // #if HLSL_DEBUG_FILL
		}
#if HLSL_DEBUG_FILL
		else
		{
			InterlockedAdd( gridIndirectArgs_conflictCount, 1 );
		}			
#endif // #if HLSL_DEBUG_FILL
	}
	else if ( blockID == HLSL_GRID_BLOCK_SETTING )
	{
		do
		{
			InterlockedCompareExchange( gridBlockIDs[gridBlockPos], HLSL_GRID_BLOCK_INVALID, HLSL_GRID_BLOCK_SETTING, blockID );
		} while ( blockID == HLSL_GRID_BLOCK_SETTING );
#if HLSL_DEBUG_FILL
		InterlockedAdd( gridIndirectArgs_waitCount1, 1 );			
#endif // #if HLSL_DEBUG_FILL
	}
#if HLSL_DEBUG_FILL
	else
	{
		InterlockedAdd( gridIndirectArgs_reuseCount, 1 );			
	}
#endif // #if HLSL_DEBUG_FILL
}

void FillCell( float3 gridPos, float3 cubeColor )
{
	const int3 gridSignedCoords = int3( gridPos ) - HLSL_GRID_HALF_WIDTH;	
	const float3 pixelRelativePos = frac( gridPos );

	if ( any( abs( gridSignedCoords ) >= HLSL_GRID_QUARTER_WIDTH ) && all( abs( gridSignedCoords ) < HLSL_GRID_HALF_WIDTH ) )
	{
#if 0
		const uint2 coverage = ComputeCoverage( pixelRelativePos, pixelRadius );
		const uint weight = countbits( coverage.x ) + countbits( coverage.y );
		if ( weight > 0 )
#else
		const uint weight = 1;
#endif
		{
			uint blockID;
			uint cellPos;
			GetBlockIDAndCellPos( gridSignedCoords + HLSL_GRID_HALF_WIDTH, blockID, cellPos );
			const float scaleFactor = 255.0f * weight;
#if 1
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].r, uint( cubeColor.r * scaleFactor ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].g, uint( cubeColor.g * scaleFactor ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].b, uint( cubeColor.b * scaleFactor ) );
#elif 0
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].r, uint( (DTid.x << 5) & 255 ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].g, uint( (DTid.y << 5) & 255 ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].b, uint( (DTid.z << 5) & 255 ) );
#elif 1
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].r, ((weight<<2)-1) * weight );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].g, ((weight<<2)-1) * weight );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].b, ((weight<<2)-1) * weight );
#endif
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].a, weight );
		}
	}
}

[numthreads( 16, 16, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{		
	
	{
		const int4 coords = int4( DTid.x, DTid.y, DTid.z, 0 );
		const float3 cubeColor = colors.Load( coords ).rgb;
		const float cubeDepth = 1.0f / ( mad ( depths.Load( coords ), depthLinearScale, depthLinearBias ) );

		const float3 lookAt = lookAts[DTid.z];
		const float3 up = ups[DTid.z];
		const float3 right = cross( lookAt, up );
		const float2 scale = (DTid.xy + 0.5) * invFrameSize * 2.0 - 1.0;
		const float3 dir = lookAt + right * scale.x - up * scale.y;	
		const float3 pos = (dir * cubeDepth) + offset;	
		const float3 gridPos = (1.0 + pos * invGridScale) * HLSL_GRID_HALF_WIDTH;
		const float cellRadius = 0.5 * cubeDepth * invFrameSize * invGridScale * HLSL_GRID_HALF_WIDTH;

#if 0
		[unroll]
		for ( uint sample = 0; sample < 8 ; ++sample )
		{
			float3 cornerPos = gridPos;
			cornerPos.x += (sample & 1) ? -cellRadius : cellRadius;
			cornerPos.y += (sample & 2) ? -cellRadius : cellRadius;
			cornerPos.z += (sample & 4) ? -cellRadius : cellRadius;
			FillCell( cornerPos, cubeColor );
		}
#else
		FillCell( gridPos, cubeColor );
#endif
	}

	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		for ( uint bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket )
			gridIndirectArgs_packedBlockCounts( bucket ) = 0;
		gridIndirectArgs_cellCount = 0;
	}
}