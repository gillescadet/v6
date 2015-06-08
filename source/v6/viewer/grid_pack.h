/*V6*/

#ifndef __V6_HLSL_GRID_PACK_H__
#define __V6_HLSL_GRID_PACK_H__

#define HLSL

#include "common_shared.h"

StructuredBuffer< GridBlockColor > gridBlockColors					: register( HLSL_GRIDBLOCK_COLOR_SRV );

RWBuffer< uint > gridIndirectArgs									: register( HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV );
RWStructuredBuffer< GridBlockPackedColor > gridBlockPackedColors	: register( HLSL_GRIDBLOCK_PACKEDCOLOR_UAV );

#endif // __V6_HLSL_GRID_PACK_H__
