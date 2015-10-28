#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)
#define GRID_CELL_MASK		(GRID_CELL_COUNT-1)

Buffer< uint > blockColors			: register( HLSL_BLOCK_COLOR_SRV );
Buffer< uint > blockIndirectArgs 	: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );

struct BlockCell
{	
	float3	posOS;
	float	halfCellSize;
	uint	color;
	uint	mip;
	uint	occupancy;
};

bool PackedColor_Unpack( uint packedID, out BlockCell o )
{
	o = (BlockCell)0;

	const uint blockID = packedID >> GRID_CELL_SHIFT;
	const uint packedOffset = block_packedOffset( GRID_CELL_BUCKET );	
	const uint packedCount = 1 + GRID_CELL_COUNT * HLSL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	
	const uint packedRank = packedBaseID + 1 + (packedID & GRID_CELL_MASK) * HLSL_COUNT;
	const uint packedColor = blockColors[packedRank + 0];

	if ( packedColor == HLSL_GRID_BLOCK_CELL_EMPTY )
	{
		return false;
	}
	else
	{
		o.color = packedColor | 0xFF;

		const uint packedPos = blockColors[packedBaseID];
		o.mip = packedPos >> 28;
		o.occupancy = blockColors[packedRank + 1];

		const uint blockPos = packedPos & 0x0FFFFFFF;
		const uint cellPos = packedColor & 0x3F;
		const uint x = (((blockPos >> 0						 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> 0						) & HLSL_GRID_BLOCK_MASK);
		const uint y = (((blockPos >> HLSL_GRID_MACRO_SHIFT	 ) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_SHIFT	) & HLSL_GRID_BLOCK_MASK);
		const uint z = (((blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT) | ((cellPos >> HLSL_GRID_BLOCK_2XSHIFT	) & HLSL_GRID_BLOCK_MASK);
		const int4 cellCoords = int4( x, y, z, 0 );	
		const float gridScale = c_blockGridScales[o.mip].x;
		o.halfCellSize = gridScale * HLSL_GRID_INV_WIDTH;
		o.posOS = mad( cellCoords.xyz, o.halfCellSize * 2.0, -gridScale + o.halfCellSize ) + c_blockCenter;

		return true;
	}	
}