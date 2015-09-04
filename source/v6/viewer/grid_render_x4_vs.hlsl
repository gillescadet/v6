#include "grid_render.hlsli"

Buffer< uint > gridBlockPackedColors : register( HLSL_GRIDBLOCK_PACKEDCOLOR4_SRV );

#define GRID_CELL_BUCKET 0
#include "grid_render_vs_impl.hlsli"
