#include "basic.hlsli"

PixelInput main( VertexInput i )
{
	PixelInput o;

	float4 pos = float4( i.position, 1.0f );

	o.position = mul( c_basicObjectToView, pos );
	o.position = mul( c_basicViewToProj, o.position );

	o.color = i.color;

	return o;
}