#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

PixelInput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID  )
{
	PixelInput o;

	const uint blockID = instanceID >> GRID_CELL_SHIFT;
	const uint packedRank = 1 + (instanceID & GRID_CELL_MASK);
	const uint packedID = blockID * (GRID_CELL_COUNT + 1);
	const uint packedColor = gridBlockPackedColors[packedID + packedRank];

	// Discard empty cell
	if ( packedColor == HLSL_GRID_BLOCK_CELL_EMPTY )
	{
		o.color = (float3)0;
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );
		return o;
	}

	const float normalizationRatio = 1.0 / 255.0;
	o.color = float3( (packedColor >> 24) & 0xFF, (packedColor >> 16) & 0xFF, (packedColor >> 8) & 0xFF ) * normalizationRatio;

	const uint blockPos = gridBlockPackedColors[packedID];
	const uint cellPos = packedColor & 0xFF;

	const uint x = (((blockPos >> 0						) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
	const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
	const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);

	const int4 gridCoords = int4( x, y, z, 0 );	

	const float halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
	float3 posOS = mad( gridCoords.xyz, halfCellSize * 2.0, -gridScale + halfCellSize );

#if HLSL_GRIDBLOCK_USE_POINTS == 0
	posOS.x += ((vertexID & 1) == 0) ? -halfCellSize : halfCellSize;
	posOS.y += ((vertexID & 2) == 0) ? -halfCellSize : halfCellSize;
	posOS.z += ((vertexID & 4) == 0) ? -halfCellSize : halfCellSize;
#endif
	
	const float4 posVS = mul( objectToView, float4( posOS, 1.0 ) );
	const float4 posCS = mul( viewToProj, posVS );

	o.position = posCS;
	
	return o;
}