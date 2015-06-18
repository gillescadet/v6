#include "grid_render.h"

Buffer< uint > gridBlockPackedColors : register( HLSL_GRIDBLOCK_PACKEDCOLOR16_SRV );

#define GRID_CELL_BUCKET 2
#include "grid_render_vs_impl.h"
