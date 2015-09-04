#include "grid_render.hlsli"

Buffer< uint > gridBlockPackedColors : register( HLSL_GRIDBLOCK_PACKEDCOLOR32_SRV );

#define GRID_CELL_BUCKET 3
#include "grid_render_vs_impl.hlsli"
