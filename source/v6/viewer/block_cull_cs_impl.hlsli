#define HLSL

#include "common_shared.h"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)

Buffer< uint > blockColors			: register( HLSL_BLOCK_COLOR_SRV );
Buffer< uint > blockIndirectArgs	: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );
RWBuffer< uint > culledBlockColors	: register( HLSL_TRACE_CULLED_BLOCK_COLOR_UAV );
RWBuffer< uint > traceIndirectArgs	: register( HLSL_TRACE_INDIRECT_ARGS_UAV );

[ numthreads( HLSL_BLOCK_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if ( DTid.x == 0 )
	{
		trace_culledCellGroupCountY = 1;
		trace_culledCellGroupCountZ = 1;
	}

	const uint blockID = DTid.x;
	const uint packedOffset = block_packedOffset( GRID_CELL_BUCKET );	
	const uint packedCount = 1 + GRID_CELL_COUNT * HLSL_COUNT;
	const uint packedBaseID = packedOffset + blockID * packedCount;	

	const uint packedPos = blockColors[packedBaseID];
	const uint mip = packedPos >> 28;
	const uint blockPos = packedPos & 0x0FFFFFFF;
	const uint xMin = ((blockPos >> 0)							& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
	const uint yMin = ((blockPos >> HLSL_GRID_MACRO_SHIFT)		& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
	const uint zMin = ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK) << HLSL_GRID_BLOCK_SHIFT;
	const int4 cellMinCoords = int4( xMin, yMin, zMin, 0 );	
	const float gridScale = c_blockGridScales[mip].x;
	const float cellSize = gridScale * 2.0f * HLSL_GRID_INV_WIDTH;
	const float3 posMinWS = mad( cellMinCoords.xyz, cellSize, -gridScale ) + c_blockCenter;
	const float3 deltaWS = cellSize * HLSL_GRID_BLOCK_WIDTH;

	const float3 vertices[8] =
	{
		float3( 0.0f, 0.0f, 0.0f ),
		float3( 0.0f, 0.0f, 1.0f ),
		float3( 0.0f, 1.0f, 0.0f ),
		float3( 0.0f, 1.0f, 1.0f ),
		float3( 1.0f, 0.0f, 0.0f ),
		float3( 1.0f, 0.0f, 1.0f ),
		float3( 1.0f, 1.0f, 0.0f ),
		float3( 1.0f, 1.0f, 1.0f ),
	};

	float2 minScreenPos = float2(  1e32f,  1e32f );
	float2 maxScreenPos = float2( -1e32f, -1e32f );
	
	const matrix worldToProjMatrix = mul( c_blockViewToProj, c_blockObjectToView );

	uint clippedCount = 0;
	for ( uint vertexID = 0; vertexID < 8; ++vertexID )
	{
		const float3 posWS = posMinWS + deltaWS * vertices[vertexID];
		const float4 posCS = mul( worldToProjMatrix, float4( posWS, 1.0f ) );
		clippedCount += (abs( posCS.x ) > posCS.w || abs( posCS.y ) > posCS.w || posCS.w < 0) ? 1 : 0;
	}

	if ( clippedCount < 8 )
	{
		uint culledBlockID;		                
		InterlockedAdd( trace_culledBlockCount, 1, culledBlockID );
		
		const uint culledBlockCount = culledBlockID + 1;
		const uint culledInstanceCount = culledBlockCount * GRID_CELL_COUNT;
		const uint culledGroupCount = GROUP_COUNT( culledInstanceCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
		InterlockedMax( trace_culledCellGroupCountX, culledGroupCount );

		const uint culledPackedBaseID = culledBlockID * packedCount;	

		for ( uint packedOffset = 0; packedOffset < packedCount; ++packedOffset )
			culledBlockColors[culledPackedBaseID + packedOffset] = blockColors[packedBaseID + packedOffset];		
	}
}
