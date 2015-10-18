#include "fake_cube.hlsli"

PixelInput main( uint cornerID : SV_VertexID )
{		
	PixelInput o = (PixelInput)0;

#if 0
	const float3 camDir = -c_basicObjectToView[2].xyz;
	uint nearestVertexID = dot( step( camDir, 0.0f ), float3( 1.0f, 2.0f, 4.0f ) );

#if 1
	const float vertexMap[8][6] = 
	{
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 1, 0, 5, 2, 7, 6 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0 },
	};

	vertexID = vertexMap[nearestVertexID][vertexID];
#endif

	float3 posOS = float3( 0.0f, 0.0f, 0.0f );

	posOS.x += ((vertexID & 1) == 0) ? -1.0f : 1.0f;
	posOS.y += ((vertexID & 2) == 0) ? -1.0f : 1.0f;
	posOS.z += ((vertexID & 4) == 0) ? -1.0f : 1.0f;

	const float4 posVS = mul( c_basicObjectToView, float4( posOS, 1.0 ) );
	const float4 posCS = mul( c_basicViewToProj, posVS );

	o.color.r = ((vertexID & 1) == 0) ? 0.0f : 0.1f;
	o.color.g = ((vertexID & 2) == 0) ? 0.0f : 0.1f;
	o.color.b = ((vertexID & 4) == 0) ? 0.0f : 0.1f;

#if 0
	if ( vertexID == nearestVertexID )
	{
		o.color.r = 1.0f;
		o.color.g = 0.0f;
		o.color.b = 0.0f;
	}
#endif

	o.position = posCS;

#else

	const float3 vertices[8] =
	{
		float3( -1.0f, -1.0f, -1.0f ),
		float3( -1.0f, -1.0f,  1.0f ),
		float3( -1.0f,  1.0f, -1.0f ),
		float3( -1.0f,  1.0f,  1.0f ),
		float3(  1.0f, -1.0f, -1.0f ),
		float3(  1.0f, -1.0f,  1.0f ),
		float3(  1.0f,  1.0f, -1.0f ),
		float3(  1.0f,  1.0f,  1.0f ),
	};

	float2 minScreenPos = float2(  1e32f,  1e32f );
	float2 maxScreenPos = float2( -1e32f, -1e32f );
	float minZ = 0.0f;
	float minW = 1e32f;

	const matrix mx = mul( c_basicViewToProj, c_basicObjectToView );
	// const matrix mx = c_basicObjectToProj;
	for ( uint vertexID = 0; vertexID < 8; ++vertexID )
	{
		const float4 posCS = mul( mx, float4( vertices[vertexID], 1.0 ) );
		const float2 screenPos = posCS.xy * rcp( posCS.w );
		if ( posCS.w < minW )
		{
			minZ = posCS.z;
			minW = posCS.w;
		}
		minScreenPos = min( minScreenPos, screenPos );
		maxScreenPos = max( maxScreenPos, screenPos );
	}

	const float scale = 0.5f * minW;
	const float2 posCS = ( minScreenPos + maxScreenPos ) * scale;
	const float2 screenOffset = ( maxScreenPos - minScreenPos ) * scale;
	
	o.position.x = posCS.x + (((cornerID & 1) == 0) ? -screenOffset.x : screenOffset.x);
	o.position.y = posCS.y + (((cornerID & 2) == 0) ? -screenOffset.y : screenOffset.y);
	o.position.z = minZ;
	o.position.w = minW;

	o.color.r = ((cornerID & 1) == 0) ? 0.2f : 0.8f;	
	o.color.g = ((cornerID & 2) == 0) ? 0.2f : 0.8f;
	o.color.b = 0.0f;

#endif

	return o;
}