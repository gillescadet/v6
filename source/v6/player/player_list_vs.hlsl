#include "player_list.hlsli"

PixelInput main_player_list_vs( VertexInput i )
{
	PixelInput o;

	const float2 posWS = mad( i.position.xy, c_listPosAndScale.w, c_listPosAndScale.xy );
	float4 posCS = float4( (posWS * c_listScreenInvSize) * 2.0f - 1.0f, i.position.z, 1.0f );
	posCS.y *= -1.0f;

	o.position = posCS;
	o.uv = i.uv;

	return o;
}
