#define HLSL
#include "trace_shared.h"

SamplerState bilinearSampler			: REGISTER_SAMPLER( HLSL_BILINEAR_SLOT );

Texture2D< float4 > inputColors			: REGISTER_SRV( HLSL_COLOR_SLOT );
Buffer< uint > inputDisplacements		: REGISTER_SRV( HLSL_DISPLACEMENT_SLOT );
Texture2D< float4 > inputHistory		: REGISTER_SRV( HLSL_HISTORY_SLOT );

RWTexture2D< float4 > outputColors		: REGISTER_UAV( HLSL_COLOR_SLOT );

float3 RGBtoYCoGg( float3 color )
{
	float3 output;

	output.y = color.r - color.b;
	const float t = color.b + output.y * 0.5f;
	output.z = color.g - t;
	output.x = t + output.z * 0.5f;

	return output;
}

float3 YCoGgtoRGB( float3 color )
{
	float3 output;

	const float t = color.x - color.z * 0.5f;
	output.g = color.z + t;
	output.b = t - color.y * 0.5f;
	output.r = color.y + output.b;

	return output;
}

[ numthreads( 8, 8, 1 ) ]
void main_pixel_tsaa_cs( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID )
{
	const float2 inputUnjitter = 1.0f - float2( c_tsaaJitter.x, 1.0f - c_tsaaJitter.y );

	const uint displacementPixelID = mad( DTid.y, c_tsaaFrameSize.x, DTid.x );
	const uint displacementF16 = inputDisplacements[displacementPixelID];
	const float2 displacement = f16tof32( uint2( displacementF16 >> 16, displacementF16 & 0xFFFF ) ) * c_tsaaFrameSize;

	const float2 curColorCoords = float2( DTid.x, DTid.y ) + inputUnjitter;
	const float2 prevColorCoords = float2( DTid.x, DTid.y ) + float2( -displacement.x, displacement.y ) + float2( 0.5f, 0.5f );
	
	const float2 curColorUVs = curColorCoords * c_tsaaInvFrameSize;
	const float2 prevColorUVs = prevColorCoords * c_tsaaInvFrameSize;

	float blendFactor = c_tsaaBlendFactor;
	if ( any( prevColorUVs != saturate( prevColorUVs) ) )
		blendFactor = 1.0f;

	const float3 curColor = RGBtoYCoGg( inputColors.SampleLevel( bilinearSampler, curColorUVs, 0 ).rgb );

	const float3 crossColor1 = RGBtoYCoGg( inputColors[uint2( DTid.x-1, DTid.y   )].rgb );
	const float3 crossColor2 = RGBtoYCoGg( inputColors[uint2( DTid.x+1, DTid.y   )].rgb );
	const float3 crossColor3 = RGBtoYCoGg( inputColors[uint2( DTid.x  , DTid.y-1 )].rgb );
	const float3 crossColor4 = RGBtoYCoGg( inputColors[uint2( DTid.x  , DTid.y+1 )].rgb );

	const float3 minColor = min( curColor, min( crossColor1, min( crossColor2, min( crossColor3, crossColor4 ) ) ) );
	const float3 maxColor = max( curColor, max( crossColor1, max( crossColor2, max( crossColor3, crossColor4 ) ) ) );

	const float3 prevColor = clamp( RGBtoYCoGg( inputHistory.SampleLevel( bilinearSampler, prevColorUVs, 0 ).rgb ), minColor, maxColor );
	
	outputColors[DTid.xy] = float4( YCoGgtoRGB( lerp( prevColor, curColor, blendFactor ) ), 0.0f );
}
