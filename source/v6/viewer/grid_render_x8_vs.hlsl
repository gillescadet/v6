#include "grid_render.h"

Buffer< uint > gridBlockPackedColors : register( HLSL_GRIDBLOCK_PACKEDCOLOR8_SRV );

#define GRID_CELL_BUCKET 1
#include "grid_render_vs_impl.h"
