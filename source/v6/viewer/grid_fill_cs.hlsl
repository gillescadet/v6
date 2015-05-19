#include "grid_fill.h"

static const float3 lookAts[6] = 
{
	float3( 1.0f,  0.0f,  0.0f ),
	float3( -1.0f , 0.0f, 0.0f ),
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f, -1.0f,  0.0f ), 
	float3( 0.0f,  0.0f,  1.0f ), 
	float3( 0.0f,  0.0f, -1.0f )    
};

static const float3 ups[6] =
{
	float3( 0.0f,  1.0f,  0.0f ),
	float3(  0.0f , 1.0f, 0.0f ),
	float3( 0.0f,  0.0f, -1.0f ),
	float3( 0.0f,  0.0f,  1.0f ),
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f,  1.0f,  0.0f )
};

[numthreads( 16, 16, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{		
	const int4 coords = int4( DTid.x, DTid.y, DTid.z, 0 );
	const float4 cubeColor = float4( colors.Load( coords ).rgb, 1.0 );
	const float cubeDepth = rcp( mad ( depths.Load( coords ), depthLinearScale, depthLinearBias ) );	

	const float3 lookAt = lookAts[DTid.z];
	const float3 up = ups[DTid.z];
	const float3 right = cross( lookAt, up );
	const float2 scale = (DTid.xy + 0.5 ) * invFrameSize * 2.0 - 1.0;
	const float3 dir = lookAt + right * scale.x - up * scale.y;	
	const float3 pos = (dir * cubeDepth) * invGridScale;
	if ( all( abs( pos ) < 1.0 ) )
	{		
		const int3 gridCoords = int3( mad( pos, halfGridSize, halfGridSize ) );		
		gridColors[gridCoords] = cubeColor;
	}
}