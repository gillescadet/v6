/*V6*/

#ifndef __V6_HLSL_PLAYER_SHARED_H__
#define __V6_HLSL_PLAYER_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_FRAME_METRICS_WIDTH					4096

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
	uint				c_composeFrameWidth;
	uint3				c_composeunused;
};

CBUFFER( CBFrameMetrics, 3 )
{
	uint2				c_frameMetricsRTSize;
	uint2				c_frameMetricsRTOffset;
	
	float2				c_frameMetricsScale;
	float				c_frameMetricsBias;
	uint				c_frameMetricsEnd;
};

struct FrameMetrics_s
{
	uint				drawTimeUS;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_PLAYER_SHARED_H__