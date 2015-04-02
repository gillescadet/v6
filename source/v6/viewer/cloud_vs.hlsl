#include "cloud.h"

PixelInput main( uint vertexID : SV_VertexID )
{
	PixelInput o;

	const uint j = vertexID / frameWidth;
	const uint i = vertexID - j * frameWidth;
	const float4 color = colors.Load( int3( i, j, 0 ) ); 	
	const float w = min( 0.5 * zFar, depths.Load( int3( i, j, 0 ) ) );

	const float u = 3.0 * i / frameWidth;
	const float v = 2.0 * j / frameHeight;
	
	const uint axis	= (int)u;
	const uint sign	= (int)v;

	const float x = sign ? -1.0 : 1.0;
	const float y = 2.0 * frac( u ) - 1.0;
	const float z = 2.0 * frac( v ) - 1.0;	
			
	const float3 dirWS[3] = { float3( x, y, z ), float3( z, x, y ), float3( y, z, x ) };

	const float4 posOS = float4( normalize( dirWS[axis] ) * w, 1.0 );
	const float4 posVS = mul( objectToView, posOS );
	const float4 posPS = mul( viewToProj, posVS );

	o.position = posPS;
	o.color = color;

	return o;
}