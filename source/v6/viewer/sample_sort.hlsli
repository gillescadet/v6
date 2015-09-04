/*V6*/

#ifndef __V6_HLSL_SAMPLE_SORT_H__
#define __V6_HLSL_SAMPLE_SORT_H__

#define HLSL

#include "common_shared.h"

RWStructuredBuffer< Sample > samples			: register( HLSL_SAMPLE_UAV );
RWBuffer< uint > sampleIndirectArgs				: register( HLSL_SAMPLE_INDIRECT_ARGS_UAV );

#endif // __V6_HLSL_SAMPLE_SORT_H__
