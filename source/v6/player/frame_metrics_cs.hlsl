#define HLSL
#include "player_shared.h"

StructuredBuffer< FrameMetrics_s > frameMetrics		: REGISTER_SRV( HLSL_FRAME_METRICS_SLOT );

RWTexture2D< float4 > surfaceColors					: REGISTER_UAV( HLSL_SURFACE_SLOT );

[numthreads(8, 8, 1)]
void main_frame_metrics_cs( uint3 DTid : SV_DispatchThreadID )
{
	const uint y = c_frameMetricsRTSize.y - DTid.y - 1;

	const uint cursor = (c_frameMetricsEnd - (c_frameMetricsRTSize.x - DTid.x) / (float)c_frameMetricsScale.x ) % HLSL_FRAME_METRICS_WIDTH;
	const float fade = abs( mad( frameMetrics[cursor].drawTimeUS + c_frameMetricsBias, c_frameMetricsScale.y, -(float)y ) ) < 1.0f;

	const float3 plotColor = float3( fade, fade, fade );
	const float3 midColor = float3( y == (c_frameMetricsRTSize.y >> 1), 0.0f, 0.0f );

	surfaceColors[DTid.xy + c_frameMetricsRTOffset] = float4( plotColor + midColor, 0.0f );
}
