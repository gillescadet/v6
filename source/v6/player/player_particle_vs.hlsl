#include "player_particle.hlsli"

PixelInput main_player_particle_vs( VertexInput i, uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID )
{
	PixelInput o;

	const float4 posLS = float4( i.position.xyz, 1.0f );
	float4 posCS;
	posCS.x = dot( posLS, c_particleMatRow0 );
	posCS.y = dot( posLS, c_particleMatRow1 );
	posCS.z = dot( posLS, c_particleMatRow2 );
	posCS.w = dot( posLS, c_particleMatRow3 );

	const float alpha = saturate( cos( i.params.x + i.params.y * c_particleTime ) );
	const float sizeX = i.params.z;
	const float sizeY = sizeX * c_particleHoverW;

	o.position = posCS;
	o.position.x += (vertexID & 1) ? sizeX : -sizeX;
	o.position.y += (vertexID & 2) ? sizeY : -sizeY;
	o.uv.x = (vertexID & 1) ? 0.0f : 1.0f;
	o.uv.y = (vertexID & 2) ? 0.0f : 1.0f;
	o.alpha = alpha;

	return o;
}
