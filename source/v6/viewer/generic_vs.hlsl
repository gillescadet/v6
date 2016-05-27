#include "generic.hlsli"

PixelInput main_generic_vs( VertexInput i )
{
	PixelInput o;

	const float4 pos = float4( i.position, 1.0f );	

	o.position = mul( c_genericObjectToWorld, pos );
	o.position = mul( c_genericWorldToView, o.position );
	o.position = mul( c_genericViewToProj, o.position );
	
	const float4 normal = float4( i.normal, 0.0f );

	o.normal = mul( c_genericObjectToWorld, normal ).xyz;
	
	o.uv = i.uv;

	return o;
}
