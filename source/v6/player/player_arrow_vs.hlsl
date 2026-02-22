#include "player_arrow.hlsli"

PixelInput main_player_arrow_vs( VertexInput i )
{
	PixelInput o;

	const float4 posLS = float4( i.position.xyz, 1.0f );
	float4 posCS;
	posCS.x = dot( posLS, c_arrowMatRow0 );
	posCS.y = dot( posLS, c_arrowMatRow1 );
	posCS.z = dot( posLS, c_arrowMatRow2 );
	posCS.w = dot( posLS, c_arrowMatRow3 );

	o.position = posCS;

	return o;
}
