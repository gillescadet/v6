/*V6*/

#ifndef __V6_HLSL_GRID_CLEAR_H__
#define __V6_HLSL_GRID_CLEAR_H__

#define HLSL

#include "common_shared.h"

RWStructuredBuffer< GridBlockColor > gridBlockColors		: register( HLSL_GRIDBLOCK_COLOR_UAV );

#endif // __V6_HLSL_CLEAR_GRID_H__
