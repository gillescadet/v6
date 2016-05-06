/*V6*/

#ifndef __V6_HLSL_CAPTURE_SHARED_H__
#define __V6_HLSL_CAPTURE_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_DEBUG_COLLECT							1

#define HLSL_CELL_SUPER_SAMPLING_WIDTH				3

#define HLSL_SAMPLE_THREAD_GROUP_SIZE				128

#define HLSL_CAPTURE_COLOR_SLOT						0
#define HLSL_CAPTURE_DEPTH_SLOT						1
#define HLSL_SAMPLE_SLOT							2
#define HLSL_SAMPLE_INDIRECT_ARGS_SLOT				3

CBUFFER( CBSample, 0 )
{
	float				c_sampleDepthLinearScale;
	float				c_sampleDepthLinearBias;
	uint				c_sampleGridWidth;
	float				c_sampleInvCubeSize;
	float3				c_samplePos;
	uint				c_sampleFaceID;
	float4				c_sampleMipBoundaries[HLSL_MIP_MAX_COUNT];
	float4				c_sampleInvGridScales[HLSL_MIP_MAX_COUNT];
};

struct Sample
{
	uint row0;
	uint row1;
	uint row2;
};

#define sample_groupCountX_offset							0
#define sample_groupCountY_offset							1
#define sample_groupCountZ_offset							2
#define sample_count_offset									3
#if HLSL_DEBUG_COLLECT == 1
#define sample_out_offset									4
#define sample_error_offset									5
#define sample_pixelCount_offset							6
#define sample_pixelSampleCount_offset						7
#if 0
#define sample_occupancy_offset								8
#define sample_cellCoords_offset( ID )						(9 + (ID))
#define sample_all_offset									(sample_cellCoords_offset( 144 ) + 1)
#else
#define sample_all_offset									8
#endif
#else
#define sample_all_offset									4
#endif // #if HLSL_DEBUG_COLLECT == 1

#define sample_groupCountX									sampleIndirectArgs[sample_groupCountX_offset] // ThreadGroupCountX
#define sample_groupCountY									sampleIndirectArgs[sample_groupCountY_offset] // ThreadGroupCountY
#define sample_groupCountZ									sampleIndirectArgs[sample_groupCountZ_offset] // ThreadGroupCountZ
#if HLSL_DEBUG_COLLECT == 1
#define sample_out											sampleIndirectArgs[sample_out_offset]
#define sample_error										sampleIndirectArgs[sample_error_offset]
#define sample_pixelCount									sampleIndirectArgs[sample_pixelCount_offset]
#define sample_pixelSampleCount								sampleIndirectArgs[sample_pixelSampleCount_offset]
#if 0
#define sample_occupancy									sampleIndirectArgs[sample_occupancy_offset]
#define sample_cellCoords( ID )								sampleIndirectArgs[sample_cellCoords_offset( ID )]
#endif
#endif // #if HLSL_DEBUG_COLLECT == 1
#define sample_count										sampleIndirectArgs[sample_count_offset]


END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_CAPTURE_SHARED_H__