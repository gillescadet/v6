/*V6*/

#ifndef __V6_HLSL_PLAYER_SHARED_H__
#define __V6_HLSL_PLAYER_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_BILINEAR_SLOT							0

#define HLSL_FRAME_METRICS_WIDTH					(30 * 75)

#define HLSL_SURFACE_SLOT							0
#define HLSL_LCOLOR_SLOT							1
#define HLSL_RCOLOR_SLOT							2
#define HLSL_FRAME_METRICS_SLOT						4

CBUFFER( CBBasic, 0 )
{
	row_major	matrix	c_basicObjectToView;
	row_major	matrix	c_basicViewToProj;
};

CBUFFER( CBCompose, 2 )
{
	uint				c_composeSurfaceWidth;
	uint3				c_composeunused;

	float2				c_composeFrameUVScale;
	float2				c_composeFrameUVBias;
};

CBUFFER( CBFrameMetrics, 3 )
{
	uint2				c_frameMetricsRTSize;
	uint2				c_frameMetricsRTOffset;
	
	uint				c_frameMetricsEnd;
	float				c_frameMetricsScale;
	float				c_frameMetricsBias;
	float				c_frameMetricsMarkerPad1;

	float				c_frameMetricsMarkerMin;
	float				c_frameMetricsMarkerMid;
	float				c_frameMetricsMarkerMax;
	float				c_frameMetricsMarkerPad2;
};

struct FrameMetrics_s
{
	uint				drawTimeUS;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_PLAYER_SHARED_H__