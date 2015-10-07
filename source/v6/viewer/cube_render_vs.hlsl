#include "cube_render.hlsli"

PixelInput main( VertexInput i, uint vertexID : SV_VertexID )
{		
	PixelInput o = (PixelInput)0;

	const float4 posVS = mul( c_cubeObjectToView, float4( i.position, 1.0 ) );
	o.position = mul( c_cubeViewToProj, posVS );
	o.uv = i.uv;
	o.faceID = vertexID >> 2;

	return o;
}