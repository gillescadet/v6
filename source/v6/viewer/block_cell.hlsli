#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

Buffer< uint > blockColors			: register( HLSL_BLOCK_COLOR_SRV );
Buffer< uint > blockIndirectArgs 	: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );

struct BlockCell
{
	uint	color;
	float4	posCS;
};

bool PackedColor_Unpack( uint packedID, out BlockCell o, int vertexID )
{
	o = (BlockCell)0;

	const uint blockID = packedID >> GRID_CELL_SHIFT;
	const uint packedOffset = block_packedOffset( GRID_CELL_BUCKET );	
	const uint packedCount = 1 + GRID_CELL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint packedRank = 1 + (packedID & GRID_CELL_MASK);
	const uint packedColor = blockColors[packedBaseID + packedRank];

	if ( packedColor == HLSL_GRID_BLOCK_CELL_EMPTY )
	{
		o.posCS = float4( -2.0, -2.0, -2.0, 1.0 );
		return false;
	}
	else
	{
		o.color = packedColor | 0xFF;

		const uint packedPos = blockColors[packedBaseID];
		const uint mip = ((packedPos >> 28) & 0xC) | ((packedColor >> 6) & 3);
		const uint blockPos = packedPos & 0x3FFFFFFF;
		const uint cellPos = packedColor & 0x3F;

		if ( c_blockShowMip )
		{
			o.color = (mip+1) & 1 ? (0xFF << 24) : 0;
			o.color |= (mip+1) & 2 ? (0xFF << 16) : 0;
			o.color |= (mip+1) & 4 ? (0xFF << 8) : 0;
			o.color |= 0xFF;
		}

		if ( c_blockShowOverdraw )
			o.color = 0x3F3F3FFF;

		const uint x = (((blockPos >> 0						 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
		const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
		const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);

		const int4 cellCoords = int4( x, y, z, 0 );	

		const float gridScale = c_blockGridScales[mip].x;
		const float halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
		float3 posOS = mad( cellCoords.xyz, halfCellSize * 2.0, -gridScale + halfCellSize ) + c_blockCenter;

		if ( vertexID != -1)
		{
			posOS.x += ((vertexID & 1) == 0) ? -halfCellSize : halfCellSize;
			posOS.y += ((vertexID & 2) == 0) ? -halfCellSize : halfCellSize;
			posOS.z += ((vertexID & 4) == 0) ? -halfCellSize : halfCellSize;
		}

		const float4 posVS = mul( c_blockObjectToView, float4( posOS, 1.0 ) );
		const float4 posCS = mul( c_blockViewToProj, posVS );

		o.posCS = posCS;
	
		return all( abs( o.posCS.xyz ) < o.posCS.w );
	}
}
