#include "basic.h"

PixelInput main( VertexInput i )
{
	PixelInput o;

	float4 pos = float4( i.position, 1.0f );

	o.position = mul( c_frameObjectToView, pos );
	o.position = mul( c_frameViewToProj, o.position );

	o.color = i.color;

	return o;
}