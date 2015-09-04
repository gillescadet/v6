/*V6*/

#ifndef __V6_HLSL_GRID_PACK_H__
#define __V6_HLSL_GRID_PACK_H__

#define HLSL

#include "common_shared.h"

StructuredBuffer< GridBlockColor > gridBlockColors	: register( HLSL_GRIDBLOCK_COLOR_SRV );
Buffer< uint > gridBlockPositions					: register( HLSL_GRIDBLOCK_POS_SRV );

RWBuffer< uint > gridIndirectArgs					: register( HLSL_GRIDBLOCK_INDIRECT_ARGS_UAV );
RWBuffer< uint > gridBlockPackedColors4				: register( HLSL_GRIDBLOCK_PACKEDCOLOR4_UAV );
RWBuffer< uint > gridBlockPackedColors8				: register( HLSL_GRIDBLOCK_PACKEDCOLOR8_UAV );
RWBuffer< uint > gridBlockPackedColors16			: register( HLSL_GRIDBLOCK_PACKEDCOLOR16_UAV );
RWBuffer< uint > gridBlockPackedColors32			: register( HLSL_GRIDBLOCK_PACKEDCOLOR32_UAV );
RWBuffer< uint > gridBlockPackedColors64			: register( HLSL_GRIDBLOCK_PACKEDCOLOR64_UAV );

#endif // __V6_HLSL_GRID_PACK_H__
