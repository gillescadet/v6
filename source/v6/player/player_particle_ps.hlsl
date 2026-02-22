#include "player_particle.hlsli"

float4 main_player_particle_ps( PixelInput i ) : SV_TARGET
{
	const float borderFade = 1.0f - length( i.uv * 2.0f - 1.0f );
	return float4( c_particleColor.rgb * i.alpha * borderFade, 1.0f );
}
