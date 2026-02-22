#include "player_env.hlsli"

PixelInput main_player_env_vs( VertexInput i )
{
	PixelInput o;

	const float4 posLS = float4( i.position.xyz, 1.0f );
	float4 posCS;
	posCS.x = dot( posLS, c_envMatRow0 );
	posCS.y = dot( posLS, c_envMatRow1 );
	posCS.z = dot( posLS, c_envMatRow2 );
	posCS.w = dot( posLS, c_envMatRow3 );

	o.position = posCS;
	o.uv = i.uv;

	return o;
}
