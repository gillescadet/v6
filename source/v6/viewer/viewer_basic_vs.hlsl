#include "viewer_basic.hlsli"

PixelInput main_viewer_basic_vs( VertexInput i )
{
	PixelInput o;

	float4 pos = float4( i.position, 1.0f );

	o.position = mul( c_basicObjectToView, pos );
	o.position = mul( c_basicViewToProj, o.position );

	o.color = i.color;

	return o;
}