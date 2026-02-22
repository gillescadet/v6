#define HLSL
#include "viewer_shared.h"

Texture2D< float4 > leftColors : REGISTER_SRV( HLSL_LCOLOR_SLOT );
Texture2D< float4 > rightColors : REGISTER_SRV( HLSL_RCOLOR_SLOT );

RWTexture2D< float4 > surfaceColors : REGISTER_UAV( HLSL_SURFACE_SLOT );

[numthreads(8, 8, 1)]
void main_viewer_surface_compose_cs( uint3 DTid : SV_DispatchThreadID )
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