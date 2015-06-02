/*V6*/

#ifndef __V6_HLSL_GRID_CLEAR_H__
#define __V6_HLSL_GRID_CLEAR_H__

#define HLSL

#include "common_shared.h"

RWStructuredBuffer< GridBlockColor > gridBlockColors		: register( HLSL_GRIDBLOCK_COLOR_UAV );
RWStructuredBuffer< uint > gridBlockAssignedIDs				: register( HLSL_GRIDBLOCK_ASSIGNED_ID_UAV );
RWStructuredBuffer< GridIndirectArgs > gridIndirectArgs		: register( HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV );

#endif // __V6_HLSL_CLEAR_GRID_H__
