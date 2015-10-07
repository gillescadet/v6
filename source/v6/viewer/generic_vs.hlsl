#include "generic.hlsli"

PixelInput main( VertexInput i )
{
	PixelInput o;

	float4 pos = float4( i.position, 1.0f );

	o.position = mul( c_genericObjectToView, pos );
	o.position = mul( c_genericViewToProj, o.position );

	o.color = i.color;
	o.uv = i.uv;

	return o;
}
