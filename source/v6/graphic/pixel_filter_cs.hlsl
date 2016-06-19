#define HLSL
#include "trace_shared.h"

SamplerState bilinearSampler			: REGISTER_SAMPLER( HLSL_BILINEAR_SLOT );

Texture2D< float4 > inputColors			: REGISTER_SRV( HLSL_COLOR_SLOT );
Buffer< uint > inputDisplacements		: REGISTER_SRV( HLSL_DISPLACEMENT_SLOT );
Texture2D< float4 > inputHistory		: REGISTER_SRV( HLSL_HISTORY_SLOT );

RWTexture2D< float4 > outputColors		: REGISTER_UAV( HLSL_COLOR_SLOT );

[ numthreads( 8, 8, 1 ) ]
void main_pixel_filter_cs( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID )
{
	const float2 inputUnjitter = 1.0f - float2( c_filterJitter.x, 1.0f - c_filterJitter.y );

	const uint displacementPixelID = mad( DTid.y, c_filterFrameSize.x, DTid.x );
	const uint displacementF16 = inputDisplacements[displacementPixelID];
	const float2 displacement = f16tof32( uint2( displacementF16 >> 16, displacementF16 & 0xFFFF ) ) * c_filterFrameSize;

	const float2 curColorCoords = float2( DTid.x, DTid.y ) + inputUnjitter;
	const float2 prevColorCoords = float2( DTid.x, DTid.y ) + float2( -displacement.x, displacement.y ) + float2( 0.5f, 0.5f );
	
	const float2 curColorUVs = curColorCoords * c_filterInvFrameSize;
	const float2 prevColorUVs = prevColorCoords * c_filterInvFrameSize;

	float blendFactor = c_filterBlendFactor;
	if ( any( prevColorUVs != saturate( prevColorUVs) ) )
		blendFactor = 1.0f;

	const float3 prevColor = inputHistory.SampleLevel( bilinearSampler, prevColorUVs, 0 ).rgb;
	// const float3 prevColor = inputHistory.Load( int3( DTid.xy, 0 ) ).rgb;
	const float3 curColor = inputColors.SampleLevel( bilinearSampler, curColorUVs, 0 ).rgb;

	outputColors[DTid.xy] = float4( lerp( prevColor, curColor, c_filterBlendFactor ), 0.0f );
}
