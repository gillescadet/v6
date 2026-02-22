/*V6*/

#ifndef __V6_HLSL_PLAYER_SHARED_H__
#define __V6_HLSL_PLAYER_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_BILINEAR_SLOT							0
#define HLSL_TRILINEAR_SLOT							1

#define HLSL_FRAME_METRICS_WIDTH					(46 * 90) // keep a multiple of the frame rate to prevent artifacts when wrapping

#define HLSL_SURFACE_SLOT							0
#define HLSL_LCOLOR_SLOT							1
#define HLSL_RCOLOR_SLOT							2
#define HLSL_FRAME_METRICS_TIME_SLOT				3
#define HLSL_FRAME_METRICS_EVENT_SLOT				4

#define HLSL_ALBEDO_SLOT							0
#define HLSL_OVERLAY_SLOT							1

CBUFFER( CBArrow, 0 )
{
	float4				c_arrowMatRow0;
	float4				c_arrowMatRow1;
	float4				c_arrowMatRow2;
	float4				c_arrowMatRow3;

	float4				c_arrowColor;
};

CBUFFER( CBParticle, 1 )
{
	float4				c_particleMatRow0;
	float4				c_particleMatRow1;
	float4				c_particleMatRow2;
	float4				c_particleMatRow3;

	float4				c_particleColor;
	
	float				c_particleTime;
	float				c_particleHoverW;
	float2				c_particleUnused;
};

CBUFFER( CBList, 2 )
{
	float4				c_listMatRow0;
	float4				c_listMatRow1;
	float4				c_listMatRow2;
	float4				c_listMatRow3;
	float4				c_listColor;
};

CBUFFER( CBEnv, 3 )
{
	float4				c_envMatRow0;
	float4				c_envMatRow1;
	float4				c_envMatRow2;
	float4				c_envMatRow3;
};

CBUFFER( CBCompose, 4 )
{
	uint				c_composeSurfaceWidth;
	uint3				c_composeunused;

	float2				c_composeFrameUVScale;
	float2				c_composeFrameUVBias;

	float4				c_composeBackColor;
};

CBUFFER( CBFrameMetrics, 5 )
{
	uint2				c_frameMetricsRTSize;
	uint2				c_frameMetricsRTOffset;
	
	uint				c_frameMetricsEnd;
	uint				c_frameMetricsFrameRate;
	float				c_frameMetricsVerticalScale;
	float				c_frameMetricsVerticalUnit;

	uint				c_frameMetricsEvent0Mask;
	uint				c_frameMetricsEvent0MinY;
	uint				c_frameMetricsEvent0MaxY;
	uint				c_frameMetricsEvent0Pad;
	float4				c_frameMetricsEvent0Color;

	uint				c_frameMetricsEvent1Mask;
	uint				c_frameMetricsEvent1MinY;
	uint				c_frameMetricsEvent1MaxY;
	uint				c_frameMetricsEvent1Pad;
	float4				c_frameMetricsEvent1Color;

	uint				c_frameMetricsEvent2Mask;
	uint				c_frameMetricsEvent2MinY;
	uint				c_frameMetricsEvent2MaxY;
	uint				c_frameMetricsEvent2Pad;
	float4				c_frameMetricsEvent2Color;

	uint				c_frameMetricsEvent3Mask;
	uint				c_frameMetricsEvent3MinY;
	uint				c_frameMetricsEvent3MaxY;
	uint				c_frameMetricsEvent3Pad;
	float4				c_frameMetricsEvent3Color;
};

struct FrameMetricsTime_s
{
	uint				drawTimeUS;
};

struct FrameMetricsEvent_s
{
	uint				events;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_PLAYER_SHARED_H__