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

[numthreads( 16, 16, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{		
	const int4 coords = int4( DTid.x, DTid.y, DTid.z, 0 );
	const float3 cubeColor = colors.Load( coords ).rgb;
	// todo: check if rcp is not too coarse
	const float cubeDepth = rcp( mad ( depths.Load( coords ), depthLinearScale, depthLinearBias ) );	

	const float3 lookAt = lookAts[DTid.z];
	const float3 up = ups[DTid.z];
	const float3 right = cross( lookAt, up );
	const float2 scale = (DTid.xy + 0.5) * invFrameSize * 2.0 - 1.0;
	const float3 dir = lookAt + right * scale.x - up * scale.y;	
	const float3 posWS = (dir * cubeDepth) + offset;
	const float3 pos = posWS * invGridScale;

	if ( all( abs( pos ) < 1.0 ) )
	{		
		const int3 gridCoords = int3( mad( pos, HLSL_GRID_HALF_WIDTH, HLSL_GRID_HALF_WIDTH ) );
		
		const int3 gridBlockCoords = gridCoords >> HLSL_GRID_BLOCK_SHIFT;
		const int gridBlockPos = (gridBlockCoords.z << HLSL_GRID_MACRO_2XSHIFT) | (gridBlockCoords.y << HLSL_GRID_MACRO_SHIFT) | gridBlockCoords.x;

		const int3 gridCellCoords = gridCoords & HLSL_GRID_BLOCK_MASK;
		const int cellPos = (gridCellCoords.z << HLSL_GRID_BLOCK_2XSHIFT) | (gridCellCoords.y << HLSL_GRID_BLOCK_SHIFT) | gridCellCoords.x;

		uint blockID = gridBlockIDs[gridBlockPos];
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

		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].r, uint( cubeColor.r * 255.0 ) );
		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].g, uint( cubeColor.g * 255.0 ) );
		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].b, uint( cubeColor.b * 255.0 ) );
		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].a, 1 );
	}

	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		for ( uint bucket = 0; bucket < HLSL_GRID_BUCKET_COUNT; ++bucket )
			gridIndirectArgs_packedBlockCounts( bucket ) = 0;
		gridIndirectArgs_cellCount = 0;
	}
}