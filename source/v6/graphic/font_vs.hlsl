#include "font.hlsli"

StructuredBuffer< Character > characters : REGISTER_SRV( HLSL_FONT_CHARACTER_SLOT );

PixelInput main_font_vs( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID )
{
	PixelInput o;

	const Character character = characters[instanceID];

	const float2 pos = float2( (character.posx16_posy16 >> 16) & 0xFFFF, (character.posx16_posy16 >> 0) & 0xFFFF );
	const float4 rect = float4( (character.x8_y8_w8_h8 >> 24) & 0xFF, (character.x8_y8_w8_h8 >> 16) & 0xFF, (character.x8_y8_w8_h8 >> 8) & 0xFF, (character.x8_y8_w8_h8 >> 0) & 0xFF );
	const float4 rgba = float4( (character.rgba >> 24) & 0xFF, (character.rgba >> 16) & 0xFF, (character.rgba >> 8) & 0xFF, (character.rgba >> 0) & 0xFF ) * (1.0f / 255.0f);

	const float4 posWS = float4( pos.x + ((vertexID & 1) ? rect.z : 0.0f), pos.y + ((vertexID & 2) ? -rect.w : 0.0f), 0.0f, 1.0f );
	const float4 posCS = float4( dot( posWS, c_fontMatRow0 ), dot( posWS, c_fontMatRow1 ), dot( posWS, c_fontMatRow2 ), dot( posWS, c_fontMatRow3 ) );

	const float2 uv0 = rect.xy * c_fontInvBitmapSize;
	const float2 uvSize = rect.zw * c_fontInvBitmapSize;

	o.position = float4( posCS.x, posCS.y, posCS.z, posCS.w );
	o.color = rgba;
	o.uv.x = uv0.x + ((vertexID & 1) ? uvSize.x : 0.0f);
	o.uv.y = uv0.y + ((vertexID & 2) ? uvSize.y : 0.0f);
	
	return o;
}
