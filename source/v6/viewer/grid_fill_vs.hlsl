#include "grid_fill.h"

PixelInput main( VertexInput i, uint vertexID : SV_VertexID )
{
	PixelInput o;

	const float3 right = cross( i.lookAt, i.up );

	switch ( vertexID & 3 )
	{
	case 0:
		o.pos = i.lookAt - right - i.up;
		o.uv = float2( 0.0f, 1.0f );
		o.posCS = float4( -1.0f, -1.0f, 0.0f, 1.0f );
		break;
	case 1:
		o.pos = i.lookAt - right + i.up;
		o.uv = float2( 0.0f, 0.0f );
		o.posCS = float4( -1.0f,  1.0f, 0.0f, 1.0f );
		break;
	case 2:
		o.pos = i.lookAt + right - i.up;
		o.uv = float2( 1.0f, 1.0f );
		o.posCS = float4(  1.0f, -1.0f, 0.0f, 1.0f );
		break;
	case 3:
		o.pos = i.lookAt + right + i.up;
		o.uv = float2( 1.0f, 0.0f );
		o.posCS = float4(  1.0f,  1.0f, 0.0f, 1.0f );
		break;
	}

	o.faceID = vertexID >> 2;
	o.posCS = float4(  -1.0f,  -1.0f, -1.0f, -1.0f );

	return o;
}