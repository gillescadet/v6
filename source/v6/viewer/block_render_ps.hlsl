#include "block_render.hlsli"

struct GBuffer
{
	float4	color	: SV_TARGET0;
	float2	uv		: SV_TARGET1;
};

GBuffer main( PixelInput i )
{
	GBuffer gbuffer;
	gbuffer.color = i.color;
	gbuffer.uv.x = i.uv.x - i.position.x;
	gbuffer.uv.y = i.uv.y - (c_blockFrameSize.y - i.position.y);
	return gbuffer;
}