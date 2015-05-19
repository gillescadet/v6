/*V6*/

#ifndef __V6_HLSL_GRID_FILL_H__
#define __V6_HLSL_GRID_FILL_H__

#define HLSL

#include "common_shared.h"

Texture2DArray<float4> colors : register( HLSL_COLOR_SRV );
Texture2DArray<float> depths : register( HLSL_DEPTH_SRV );
RWTexture3D<float4> gridColors : register( HLSL_GRIDCOLOR_UAV );

#endif // __V6_HLSL_GRID_FILL_H__
