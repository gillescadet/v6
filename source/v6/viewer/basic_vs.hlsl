#include "basic.h"

PixelInput main( VertexInput i )
{
	PixelInput o;

	float4 pos = float4( i.position, 1.0f );

	o.position = mul( objectToView, pos );
	o.position = mul( viewToProj, o.position );

	o.color = i.color;

	return o;
}