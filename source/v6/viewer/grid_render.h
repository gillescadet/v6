/*V6*/

#ifndef __V6_HLSL_GRID_RENDER_H__
#define __V6_HLSL_GRID_RENDER_H__

#define HLSL

#include "common_shared.h"

Buffer< uint > gridBlockPositions								: register( HLSL_GRIDBLOCK_POS_SRV );
StructuredBuffer< GridBlockPackedColor > gridBlockPackedColors	: register( HLSL_GRIDBLOCK_PACKEDCOLOR_SRV );

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 color : COLOR;
};

#endif // __V6_HLSL_GRID_RENDER_H__
