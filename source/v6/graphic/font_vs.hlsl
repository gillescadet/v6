#include "font.hlsli"

StructuredBuffer< Character > characters : REGISTER_SRV( HLSL_FONT_CHARACTER_SLOT );

PixelInput main_font_vs( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID )
{
	PixelInput o;

	const Character character = characters[instanceID];

	const float2 pos = float2( (character.posx16_posy16 >> 16) & 0xFFFF, (character.posx16_posy16 >> 0) & 0xFFFF );
	const float4 rect = float4( (character.x8_y8_w8_h8 >> 24) & 0xFF, (character.x8_y8_w8_h8 >> 16) & 0xFF, (character.x8_y8_w8_h8 >> 8) & 0xFF, (character.x8_y8_w8_h8 >> 0) & 0xFF );
	const float4 rgba = float4( (character.rgba >> 24) & 0xFF, (character.rgba >> 16) & 0xFF, (character.rgba >> 8) & 0xFF, (character.rgba >> 0) & 0xFF ) * (1.0f / 255.0f);

	const float2 clip0 = mad( pos * c_fontInvFrameSize, 2.0f, -1.0f );
	const float2 clipSize = rect.zw * c_fontInvFrameSize * 2.0f;

	const float2 uv0 = rect.xy * c_fontInvBitmapSize;
	const float2 uvSize = rect.zw * c_fontInvBitmapSize;

	o.position.x = clip0.x + (vertexID & 1 ? clipSize.x : 0.0f);
	o.position.y = (clip0.y + (vertexID & 2 ? clipSize.y : 0.0f)) * -1.0f;
	o.position.z = 0.0f;
	o.position.w = 1.0f;

	o.color = rgba;

	o.uv.x = uv0.x + (vertexID & 1 ? uvSize.x : 0.0f);
	o.uv.y = uv0.y + (vertexID & 2 ? uvSize.y : 0.0f);
	
	return o;
}
