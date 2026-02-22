#include "player_list.hlsli"

PixelInput main_player_list_vs( VertexInput i )
{
	PixelInput o;

	const float4 posLS = float4( i.position.xyz, 1.0f );
	float4 posCS;
	posCS.x = dot( posLS, c_listMatRow0 );
	posCS.y = dot( posLS, c_listMatRow1 );
	posCS.z = dot( posLS, c_listMatRow2 );
	posCS.w = dot( posLS, c_listMatRow3 );

	o.position = posCS;
	o.uv = i.uv;

	return o;
}
