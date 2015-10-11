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
	const uint packedOffset = block_packedOffset( GRID_CELL_BUCKET );	
	const uint packedCount = 1 + GRID_CELL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint packedRank = 1 + (instanceID & GRID_CELL_MASK);
	const uint packedColor = blockColors[packedBaseID + packedRank];

	// Discard empty cell
	if ( packedColor == HLSL_GRID_BLOCK_CELL_EMPTY )
	{
		o.color = (float4)0;
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );
		o.uv = float2( 0.0, 0.0 );
		return o;
	}

	const float normalizationRatio = 1.0 / 255.0;
	o.color.rgb = float3( (packedColor >> 24) & 0xFF, (packedColor >> 16) & 0xFF, (packedColor >> 8) & 0xFF ) * normalizationRatio;

	const uint packedPos = blockColors[packedBaseID];
	const uint mip = ((packedPos >> 28) & 0xC) | ((packedColor >> 6) & 3);
	const uint blockPos = packedPos & 0x3FFFFFFF;
	const uint cellPos = packedColor & 0x3F;

	o.color.a = float( mip + 1 ) / float( HLSL_MIP_MAX_COUNT );

	if ( c_blockShowMip )
	{
		o.color.r = (mip+1) & 1 ? 255 : 0;
		o.color.g = (mip+1) & 2 ? 255 : 0;
		o.color.b = (mip+1) & 4 ? 255 : 0;
	}

	const uint x = (((blockPos >> 0						 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
	const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
	const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);

	const int4 cellCoords = int4( x, y, z, 0 );	

	const float gridScale = c_blockGridScales[mip].x;
	const float halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
	float3 posOS = mad( cellCoords.xyz, halfCellSize * 2.0, -gridScale + halfCellSize ) + c_blockCenter;

#if HLSL_DEBUG_BLOCK == 0
	posOS.x += ((vertexID & 1) == 0) ? -halfCellSize : halfCellSize;
	posOS.y += ((vertexID & 2) == 0) ? -halfCellSize : halfCellSize;
	posOS.z += ((vertexID & 4) == 0) ? -halfCellSize : halfCellSize;
#endif
	
	const float4 posVS = mul( c_blockObjectToView, float4( posOS, 1.0 ) );
	const float4 posCS = mul( c_blockViewToProj, posVS );

	o.position = posCS;
	o.uv = mad( posCS.xy / posCS.w, 0.5f, 0.5f ) * c_blockFrameSize;
	
	return o;
}