#define HLSL
#include "player_shared.h"

StructuredBuffer< FrameMetrics_s > frameMetrics		: REGISTER_SRV( HLSL_FRAME_METRICS_SLOT );

RWTexture2D< float4 > surfaceColors					: REGISTER_UAV( HLSL_SURFACE_SLOT );

float TimeUSToY( float timeUS )
{
	return (timeUS + c_frameMetricsBias) * c_frameMetricsScale;
}

[numthreads(8, 8, 1)]
void main_frame_metrics_cs( uint3 DTid : SV_DispatchThreadID )
{
	const uint y = c_frameMetricsRTSize.y - DTid.y - 1;

	const uint cursor = (HLSL_FRAME_METRICS_WIDTH + c_frameMetricsEnd - c_frameMetricsRTSize.x + DTid.x ) % HLSL_FRAME_METRICS_WIDTH;
	const uint cursorDistance = (HLSL_FRAME_METRICS_WIDTH + c_frameMetricsEnd - cursor) % HLSL_FRAME_METRICS_WIDTH;

	float t;
	if ( cursorDistance < 4 )
	{
		t = -1000.0f;
	}
	else
	{
		t = 0.0f;
		for ( uint rank = 0; rank < 4; ++rank )
		{
			const uint slot = (cursor & ~3) + rank;
			t += TimeUSToY( frameMetrics[slot].drawTimeUS );
		}
		t *= 0.25f;
	}

	t = min( t, c_frameMetricsRTSize.y - 1 );
	const float plotFade = 1.0f - saturate( abs( t - y ) * 0.5f );

	const float tMin = TimeUSToY( c_frameMetricsMarkerMin );
	const float tMid = TimeUSToY( c_frameMetricsMarkerMid );
	const float tMax = TimeUSToY( c_frameMetricsMarkerMax );

	const float plotMin = 1.0f - saturate( abs( tMin - y ) );
	const float plotMid = 1.0f - saturate( abs( tMid - y ) );
	const float plotMax = 1.0f - saturate( abs( tMax - y ) );

	float3 finalColor = float3( plotFade, plotFade, plotFade );
	finalColor.r += plotMax;
	finalColor.g += plotMin;
	finalColor.b += plotMid;

	if ( (cursor % 75) == 0 )
		finalColor += float3( 0.1f, 0.1f, 0.1f );

	surfaceColors[DTid.xy + c_frameMetricsRTOffset] = float4( finalColor, 0.0f );
}
