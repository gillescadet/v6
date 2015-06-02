#include "grid_render.h"

PixelInput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID  )
{
	PixelInput o;

	const uint gridBlockID = instanceID >> HLSL_GRID_BLOCK_3XSHIFT;
	const uint cellPos = instanceID & HLSL_GRID_BLOCK_CELL_POS_MASK;
		
	const GridPackedColor packedColor = gridBlockPackedColors[gridBlockID].colors[cellPos];

	[branch]
	if ( (packedColor.rgba & 0xFF) == 0 )
	{
		o.color = (float3)0;
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );
		return o;
	}
		
	const float normalizationRatio = 1.0 / 255.0;
	o.color = float3( (packedColor.rgba >> 24) & 0xFF, (packedColor.rgba >> 16) & 0xFF, (packedColor.rgba >> 8) & 0xFF ) * normalizationRatio;

	uint gridPosBits = (gridBlockPackedColors[gridBlockID].blockPos << HLSL_GRID_BLOCK_3XSHIFT) | cellPos;
	const int x = gridPosBits & HLSL_GRID_MASK;
	gridPosBits >>= HLSL_GRID_SHIFT;
	const uint y = gridPosBits & HLSL_GRID_MASK;
	gridPosBits >>= HLSL_GRID_SHIFT;
	const int z = gridPosBits;

	const int4 gridCoords = int4( x, y, z, 0 );	

	const float halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
	float3 posOS = mad( gridCoords.xyz, halfCellSize * 2.0, -gridScale + halfCellSize );

	posOS.x += ((vertexID & 1) == 0) ? -halfCellSize : halfCellSize;
	posOS.y += ((vertexID & 2) == 0) ? -halfCellSize : halfCellSize;
	posOS.z += ((vertexID & 4) == 0) ? -halfCellSize : halfCellSize;	
	
	const float4 posVS = mul( objectToView, float4( posOS, 1.0 ) );
	float4 posCS = mul( viewToProj, posVS );

	o.position = posCS;
	
	return o;
}