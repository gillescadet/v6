#include "font.hlsli"

PixelInput main_font_background_vs( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID )
{
	PixelInput o;

	const float4 posWS = float4( c_fontBackgroundQuad.x + ((vertexID & 1) ? c_fontBackgroundQuad.z : 0.0f), c_fontBackgroundQuad.y + ((vertexID & 2) ? c_fontBackgroundQuad.w : 0.0f), 0.0f, 1.0f );
	const float4 posCS = float4( dot( posWS, c_fontMatRow0 ), dot( posWS, c_fontMatRow1 ), dot( posWS, c_fontMatRow2 ), dot( posWS, c_fontMatRow3 ) );

	o.position = float4( posCS.x, posCS.y, posCS.z, posCS.w );
	o.color = c_fontBackgroundColor;
	o.uv.x = (vertexID & 1) ? 1.0f : 0.0f;
	o.uv.y = (vertexID & 2) ? 1.0f : 0.0f;
	
	return o;
}
