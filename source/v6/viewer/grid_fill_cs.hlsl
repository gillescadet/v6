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
	const float cubeDepth = rcp( mad ( depths.Load( coords ), depthLinearScale, depthLinearBias ) );	

	const float3 lookAt = lookAts[DTid.z];
	const float3 up = ups[DTid.z];
	const float3 right = cross( lookAt, up );
	const float2 scale = (DTid.xy + 0.5) * invFrameSize * 2.0 - 1.0;
	const float3 dir = lookAt + right * scale.x - up * scale.y;	
	const float3 pos = (dir * cubeDepth) * invGridScale;
	if ( all( abs( pos ) < 1.0 ) )
	{		
		const int3 gridCoords = int3( mad( pos, HLSL_GRID_HALF_WIDTH, HLSL_GRID_HALF_WIDTH ) );
		
		const int3 gridBlockCoords = gridCoords >> HLSL_GRID_BLOCK_SHIFT;
		const int gridBlockPos = (gridBlockCoords.z << HLSL_GRID_MACRO_2XSHIFT) | (gridBlockCoords.y << HLSL_GRID_MACRO_SHIFT) | gridBlockCoords.x;

		uint blockID = gridBlockAssignedIDs[gridBlockPos];
		if ( blockID == HLSL_GRID_BLOCK_INVALID )
		{
			blockID = gridBlockAssignedIDs.IncrementCounter()-1;
			uint prevBlockID;
			InterlockedCompareExchange( gridBlockAssignedIDs[gridBlockPos], HLSL_GRID_BLOCK_INVALID, blockID, prevBlockID );
			if ( prevBlockID != HLSL_GRID_BLOCK_INVALID )
				blockID = prevBlockID;
		}

		const int3 gridCellCoords = gridCoords & HLSL_GRID_BLOCK_MASK;
		const int cellPos = (gridCellCoords.z << HLSL_GRID_BLOCK_2XSHIFT) | (gridCellCoords.y << HLSL_GRID_BLOCK_SHIFT) | gridBlockCoords.x;

		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].r, uint( cubeColor.r * 255.0 ) );
		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].g, uint( cubeColor.g * 255.0 ) );
		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].b, uint( cubeColor.b * 255.0 ) );
		InterlockedAdd( gridBlockColors[blockID].colors[cellPos].a, 1 );
	}
}