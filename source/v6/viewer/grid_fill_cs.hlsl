#include "grid_fill.h"

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

void UnsetCoverage( inout uint2 coverage, uint i, uint j, uint k )
{
	const uint bit = k * (HLSL_GRID_BLOCK_WIDTH * HLSL_GRID_BLOCK_WIDTH) + j * HLSL_GRID_BLOCK_WIDTH + i;
	const uint bucket = bit >> 5;
	const uint mask = (1 << (bit & 31));
	coverage[bucket] &= ~mask;
}

uint2 ComputeCoverage( float3 pixelRelativePos, float pixelRadius  )
{
	const float step = 1.0 / HLSL_GRID_BLOCK_WIDTH;
	const float halfStep = step * 0.5;
	const float limits[HLSL_GRID_BLOCK_WIDTH] = { step * 0.0f + halfStep, step * 1.0f + halfStep, step * 2.0f + halfStep, step * 3.0f + halfStep };

	uint2 coverage = uint2( 0xFFFFFFFF, 0xFFFFFFFF );

	[unroll]
	for ( uint i = 0; i < HLSL_GRID_BLOCK_WIDTH; ++i )
	{
		if ( limits[i] < pixelRelativePos.x - pixelRadius || limits[i] > pixelRelativePos.x + pixelRadius )
		{
			[unroll]
			for ( uint j = 0; j < HLSL_GRID_BLOCK_WIDTH; ++j )
			{
				[unroll]
				for ( uint k = 0; k < HLSL_GRID_BLOCK_WIDTH; ++k )
				{
					UnsetCoverage( coverage, i, j, k );
				}
			}
		}
	}

	[unroll]
	for ( uint j = 0; j < HLSL_GRID_BLOCK_WIDTH; ++j )
	{
		if ( limits[j] < pixelRelativePos.y - pixelRadius || limits[j] > pixelRelativePos.y + pixelRadius )
		{
			[unroll]
			for ( uint i = 0; i < HLSL_GRID_BLOCK_WIDTH; ++i )
			{
				[unroll]
				for ( uint k = 0; k < HLSL_GRID_BLOCK_WIDTH; ++k )
				{
					UnsetCoverage( coverage, i, j, k );
				}
			}
		}
	}

	[unroll]
	for ( uint k = 0; k < HLSL_GRID_BLOCK_WIDTH; ++k )
	{
		if ( limits[k] < pixelRelativePos.z - pixelRadius || limits[k] > pixelRelativePos.z + pixelRadius )
		{
			[unroll]
			for ( uint j = 0; j < HLSL_GRID_BLOCK_WIDTH; ++j )
			{
				[unroll]
				for ( uint i = 0; i < HLSL_GRID_BLOCK_WIDTH; ++i )
				{
					UnsetCoverage( coverage, i, j, k );
				}
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

[numthreads( 16, 16, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{		
	const int4 coords = int4( DTid.x, DTid.y, DTid.z, 0 );
	const float3 cubeColor = colors.Load( coords ).rgb;
	// todo: check if rcp is not too coarse
	//const float cubeDepth = rcp( mad ( depths.Load( coords ), depthLinearScale, depthLinearBias ) );	
	const float cubeDepth = 1.0f / ( mad ( depths.Load( coords ), depthLinearScale, depthLinearBias ) );	

	const float3 lookAt = lookAts[DTid.z];
	const float3 up = ups[DTid.z];
	const float3 right = cross( lookAt, up );
	const float2 scale = (DTid.xy + 0.5) * invFrameSize * 2.0 - 1.0;
	const float3 dir = lookAt + right * scale.x - up * scale.y;	
	const float3 pos = (dir * cubeDepth) + offset;	
	const float3 gridSignedPos = pos * invGridScale * HLSL_GRID_HALF_WIDTH;
	const int3 gridSignedCoords = int3( gridSignedPos );
	const float pixelRadius = cubeDepth * invFrameSize;
	const float3 pixelRelativePos = frac( gridSignedPos );

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
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].r, uint( cubeColor.r * scaleFactor ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].g, uint( cubeColor.g * scaleFactor ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].b, uint( cubeColor.b * scaleFactor ) );
			InterlockedAdd( gridBlockColors[blockID].colors[cellPos].a, weight );
		}
	}

	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		for ( uint bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket )
			gridIndirectArgs_packedBlockCounts( bucket ) = 0;
		gridIndirectArgs_cellCount = 0;
	}
}