#define HLSL
#include "trace_shared.h"

SamplerState bilinearSampler			: REGISTER_SAMPLER( HLSL_BILINEAR_SLOT );

Texture2D< float4 > inputColors			: REGISTER_SRV( HLSL_COLOR_SLOT );
Texture2D< float4 > inputDisplacements	: REGISTER_SRV( HLSL_DISPLACEMENT_SLOT );
Texture2D< float4 > inputHistory		: REGISTER_SRV( HLSL_HISTORY_SLOT );

RWTexture2D< float4 > outputColors		: REGISTER_UAV( HLSL_COLOR_SLOT );

[ numthreads( 8, 8, 1 ) ]
void main_pixel_filter_cs( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID )
{
	const int3 prevColorCoords = int3( DTid.xy, 0 );
	const float2 curColorUVs = ( float2( DTid.x, DTid.y ) + c_filterUnJitter ) * c_filterInvFrameSize;

	const float3 prevColor = inputHistory.Load( prevColorCoords ).rgb;
	const float3 curColor = inputColors.SampleLevel( bilinearSampler, curColorUVs, 0 ).rgb;

	outputColors[DTid.xy] = float4( lerp( prevColor, curColor, c_filterBlendFactor ), 0.0f );
}
