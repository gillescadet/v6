#include "block_render.hlsli"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

Buffer< uint > blockColors			: register( HLSL_BLOCK_COLOR_SRV );
Buffer< uint > blockIndirectArgs 	: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );

PixelInput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID  )
{
	PixelInput o;	

	const uint blockID = instanceID >> GRID_CELL_SHIFT;
	const uint packedOffset = block_packedOffset( c_blockCurrentMip, GRID_CELL_BUCKET );	
	const uint packedCount = 1 + GRID_CELL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint packedRank = 1 + (instanceID & GRID_CELL_MASK);
	const uint packedColor = blockColors[packedBaseID + packedRank];

	// Discard empty cell
	if ( packedColor == HLSL_GRID_BLOCK_CELL_EMPTY )
	{
		o.color = (float3)0;
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );
		return o;
	}

	const float normalizationRatio = 1.0 / 255.0;
	o.color = float3( (packedColor >> 24) & 0xFF, (packedColor >> 16) & 0xFF, (packedColor >> 8) & 0xFF ) * normalizationRatio;

	const uint blockPos = blockColors[packedBaseID];
	const uint cellPos = packedColor & 0xFF;

#if 0
	o.color.r = 0.25 + ((cellPos >> 0) & 3) * 0.25;
	o.color.g = 0.25 + ((cellPos >> 2) & 3) * 0.25;
	o.color.b = 0.25 + ((cellPos >> 4) & 3) * 0.25;
#endif

#if 1
	o.color.r = (c_blockCurrentMip+1) & 1 ? 255 : 0;
	o.color.g = (c_blockCurrentMip+1) & 2 ? 255 : 0;
	o.color.b = (c_blockCurrentMip+1) & 4 ? 255 : 0;
#endif	

	const uint x = (((blockPos >> 0						 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
	const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
	const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);

	const int4 cellCoords = int4( x, y, z, 0 );	

	const float halfCellSize = c_blockGridScale * HLSL_GRID_INV_WIDTH;
	float3 posOS = mad( cellCoords.xyz, halfCellSize * 2.0, -c_blockGridScale + halfCellSize );

#if HLSL_DEBUG_BLOCK == 1
	posOS.x += ((vertexID & 1) == 0) ? -halfCellSize : halfCellSize;
	posOS.y += ((vertexID & 2) == 0) ? -halfCellSize : halfCellSize;
	posOS.z += ((vertexID & 4) == 0) ? -halfCellSize : halfCellSize;
#endif
	
	const float4 posVS = mul( c_frameObjectToView, float4( posOS, 1.0 ) );
	const float4 posCS = mul( c_frameViewToProj, posVS );

	o.position = posCS;
	
	return o;
}