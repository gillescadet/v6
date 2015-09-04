/*V6*/

#ifndef __V6_HLSL_GRID_CLEAR_H__
#define __V6_HLSL_GRID_CLEAR_H__

#define HLSL

#include "common_shared.h"

Buffer< uint > gridBlockPositions							: register( HLSL_GRIDBLOCK_POS_SRV );
Buffer< uint > gridIndirectArgs								: register( HLSL_GRIDBLOCK_INDIRECT_ARGS_SRV );

RWBuffer< uint > gridBlockIDs								: register( HLSL_GRIDBLOCK_ID_UAV );
RWStructuredBuffer< GridBlockColor > gridBlockColors		: register( HLSL_GRIDBLOCK_COLOR_UAV );

#endif // __V6_HLSL_CLEAR_GRID_H__
