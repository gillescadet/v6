float4 main( uint vertexID : SV_VertexID ) : SV_Position
{
	if ( vertexID == 0 )
	{
		return float4( -1.0, -1.0, 0.0, 1.0 );
	}
	if ( vertexID == 1 )
	{
		return float4( -1.0, 3.0, 0.0, 1.0 );
	}
	return float4( 3.0, -1.0, 0.0, 1.0 );
}
