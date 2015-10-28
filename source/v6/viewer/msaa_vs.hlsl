#define HLSL

#include "common_shared.h"

struct PixelInput
{
					float4 position : SV_POSITION;
	nointerpolation uint2 uv		: UV;
};

PixelInput main( uint vertexID : SV_VertexID, uint instanceID: SV_InstanceID )
{
	PixelInput o = (PixelInput)0;
	
	float2 offset;
	switch( vertexID )
	{	
	case 1:
		offset = float2( 0.0f, 1.0f );
		break;
	case 2:
		offset = float2( 1.0f, 0.0f );
		break;
	case 3:
		offset = float2( 1.0f, 1.0f );
		break;
	default:
		offset = float2( 0.0f, 0.0f );
		break;
	}

	const uint w = HLSL_PIXEL_MULTISAMPLE_WIDTH;

	o.uv.x = instanceID % w;
	o.uv.y = instanceID / w;

	const float x = o.uv.x / float( w );
	const float y = o.uv.y / float( w );

	o.position.x = (x + offset.x / float( w ) ) * 2.0f - 1.0f;
	o.position.y = (y + offset.y / float( w ) ) * 2.0f - 1.0f;
	o.position.z = 0;
	o.position.w = 1;	

	return o;
}