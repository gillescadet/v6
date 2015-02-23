#include "basic.h"

PixelInput main( VertexInput i )
{
	PixelInput o;

	float4 pos = float4( i.position, 1.0f );

	o.position = mul( pos, worldToView );
	o.position = mul( o.position, viewToProj );

	o.color = i.color;

	return o;
}