#define HLSL
#include "player_shared.h"

StructuredBuffer< FrameMetricsTime_s > frameMetricsTime		: REGISTER_SRV( HLSL_FRAME_METRICS_TIME_SLOT );
StructuredBuffer< FrameMetricsEvent_s > frameMetricsEvent	: REGISTER_SRV( HLSL_FRAME_METRICS_EVENT_SLOT );

RWTexture2D< float4 > surfaceColors							: REGISTER_UAV( HLSL_SURFACE_SLOT );

float TimeUSToY( float timeUS )
{
	return timeUS * c_frameMetricsVerticalScale;
}

[numthreads(8, 8, 1)]
void main_player_frame_metrics_cs( uint3 DTid : SV_DispatchThreadID )
{
	const uint y = c_frameMetricsRTSize.y - DTid.y - 1;

	const uint cursor = (HLSL_FRAME_METRICS_WIDTH + c_frameMetricsEnd - c_frameMetricsRTSize.x + DTid.x ) % HLSL_FRAME_METRICS_WIDTH;
	const float t = min( TimeUSToY( frameMetricsTime[cursor].drawTimeUS ), c_frameMetricsRTSize.y - 1 );
	const uint events = frameMetricsEvent[cursor].events;
	const float plotFade = saturate( t - y ) * 0.25f;

	const float unitFade = y > 0 ? (1.0f - saturate( fmod( (float)y, TimeUSToY( c_frameMetricsVerticalUnit ) ) )) * 0.25f : 0.0f;

	float3 finalColor = float3( plotFade + unitFade, plotFade, plotFade );

	if ( (cursor % c_frameMetricsFrameRate) <= 2 )
		finalColor += float3( 0.05f, 0.05f, 0.05f );

	finalColor += ((events & c_frameMetricsEvent0Mask) && (y >= c_frameMetricsEvent0MinY && y <= c_frameMetricsEvent0MaxY)) ? c_frameMetricsEvent0Color.rgb : float3( 0.0f, 0.0f, 0.0f );
	finalColor += ((events & c_frameMetricsEvent1Mask) && (y >= c_frameMetricsEvent1MinY && y <= c_frameMetricsEvent1MaxY)) ? c_frameMetricsEvent1Color.rgb : float3( 0.0f, 0.0f, 0.0f );
	finalColor += ((events & c_frameMetricsEvent2Mask) && (y >= c_frameMetricsEvent2MinY && y <= c_frameMetricsEvent2MaxY)) ? c_frameMetricsEvent2Color.rgb : float3( 0.0f, 0.0f, 0.0f );
	finalColor += ((events & c_frameMetricsEvent3Mask) && (y >= c_frameMetricsEvent3MinY && y <= c_frameMetricsEvent3MaxY)) ? c_frameMetricsEvent3Color.rgb : float3( 0.0f, 0.0f, 0.0f );

	surfaceColors[DTid.xy + c_frameMetricsRTOffset] = float4( finalColor, 0.0f );
}
