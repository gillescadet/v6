#define HLSL
#include "viewer_shared.h"

Texture2D< float4 > leftColors : register( HLSL_LCOLOR_SRV );
Texture2D< float4 > rightColors : register( HLSL_RCOLOR_SRV );

RWTexture2D< float4 > surfaceColors : register( HLSL_SURFACE_UAV );

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
#if 1
	if ( DTid.x < c_composeFrameWidth )
		surfaceColors[DTid.xy] = leftColors[DTid.xy];
	else
		surfaceColors[DTid.xy] = rightColors[uint2( DTid.x-c_composeFrameWidth, DTid.y)];
#else
	if ( DTid.x < c_composeFrameWidth )
	{
		const float3 lc = leftColors[DTid.xy].xyz;
		if ( ((DTid.x+DTid.y) & 1) == 0 )
			surfaceColors[uint2( DTid.x+(c_composeFrameWidth>>1), DTid.y)] = float4( lc.r, 0.5f * lc.g, 0.0f, 1.0f );
	}
	else
	{
		const float3 rc = rightColors[uint2( DTid.x-c_composeFrameWidth, DTid.y)].xyz;
		if ( ((DTid.x+DTid.y) & 1) == 1 )
			surfaceColors[uint2( DTid.x-(c_composeFrameWidth>>1), DTid.y)] = float4( 0.0f, 0.5f * rc.g, rc.b, 1.0f );
	}
#endif
}