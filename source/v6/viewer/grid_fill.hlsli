/*V6*/

#ifndef __V6_HLSL_GRID_FILL_H__
#define __V6_HLSL_GRID_FILL_H__

#define HLSL

#include "common_shared.h"

Texture2DArray< float4 > colors							: register( HLSL_COLOR_SRV );
Texture2DArray< float > depths							: register( HLSL_DEPTH_SRV );

RWBuffer< uint > gridBlockIDs							: register( HLSL_GRIDBLOCK_ID_UAV );
RWStructuredBuffer< GridBlockColor > gridBlockColors	: register( HLSL_GRIDBLOCK_COLOR_UAV );
RWBuffer< uint > gridBlockPositions						: register( HLSL_GRIDBLOCK_POS_UAV );
RWBuffer< uint > gridIndirectArgs						: register( HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV );

#endif // __V6_HLSL_GRID_FILL_H__
