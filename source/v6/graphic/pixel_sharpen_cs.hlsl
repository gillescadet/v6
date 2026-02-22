#define HLSL
#include "trace_shared.h"

Texture2D< float4 > inputColors			: REGISTER_SRV( HLSL_COLOR_SLOT );

RWTexture2D< float4 > outputColors		: REGISTER_UAV( HLSL_COLOR_SLOT );

float Luminance( float3 color )
{
	return color.r + color.g * 2.0f + color.b;
}

[ numthreads( 8, 8, 1 ) ]
void main_pixel_sharpen_cs( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID )
{
	const uint2 pixelCoords = DTid.xy;
	
	float3 s0 = inputColors[pixelCoords].rgb;

	const float l0 = Luminance( s0 );
	const float l1 = Luminance( inputColors[uint2( pixelCoords.x-1, pixelCoords.y )].rgb );
	const float l2 = Luminance( inputColors[uint2( pixelCoords.x+1, pixelCoords.y )].rgb );
	const float l3 = Luminance( inputColors[uint2( pixelCoords.x,   pixelCoords.y-1 )].rgb );
	const float l4 = Luminance( inputColors[uint2( pixelCoords.x,   pixelCoords.y+1 )].rgb );

	const float minLuminance = min( l0, min( l1, min( l2, min( l3, l4 ) ) ) );
	const float maxLuminance = max( l0, max( l1, max( l2, max( l3, l4 ) ) ) );

	if ( minLuminance > 0.5f * maxLuminance )
	{
		const float blurryLuminance = (l1 + l2 + l3 + l4) * 0.25f;
		s0 *= (l0 * 2.0f - blurryLuminance) / l0;
	}
	
	const float3 outOfRangeColor = ((Gid.x & 1) == (Gid.y & 1)) ? float3( 0.01f, 0.01f, 0.01f ) : (float3( l0, l0, l0 ) * 0.01f);
	const float3 black = c_postProcessFadeToBlack == 0.0f ? outOfRangeColor : float3( 0.0f, 0.0f, 0.0f );
	const float3 fadeToBlack = c_postProcessFadeToBlack == 0.0f ? c_postProcessOutOfRange : c_postProcessFadeToBlack;

	outputColors[pixelCoords] = float4( lerp( s0, black, fadeToBlack ), 0.0f );
}
