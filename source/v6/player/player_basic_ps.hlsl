#include "player_basic.hlsli"

float4 main_player_basic_ps( PixelInput i ) : SV_TARGET
{
	return i.color;
}
