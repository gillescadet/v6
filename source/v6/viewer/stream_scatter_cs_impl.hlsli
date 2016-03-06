#define HLSL

#include "common_shared.h"

#define GRID_CELL_SHIFT		(GRID_CELL_BUCKET+2)
#define GRID_CELL_COUNT		(1<<GRID_CELL_SHIFT)

Buffer< uint > blockPositions			: register( HLSL_BLOCK_POS_SRV );
Buffer< uint > blockData				: register( HLSL_BLOCK_DATA_SRV );
Buffer< uint > blockIndirectArgs		: register( HLSL_BLOCK_INDIRECT_ARGS_SRV );
Buffer< uint > streamBits				: register( HLSL_STREAM_BITS_SRV );
Buffer< uint > streamAddresses			: register( HLSL_STREAM_ADDRESSES_SRV );

RWBuffer< uint > streamBlockPositions	: register( HLSL_STREAM_BLOCK_POS_UAV );
RWBuffer< uint > streamBlockData		: register( HLSL_STREAM_BLOCK_DATA_UAV );

#define V6_ASSERT

void ScatterBlock( uint packedBlockPos, uint prevBlockID, uint newBlockID )
{
	const uint posOffset = block_posOffset( GRID_CELL_BUCKET );
	const uint blockPosID = posOffset + newBlockID;
	streamBlockPositions[blockPosID] = packedBlockPos;

	const uint dataOffset = block_dataOffset( GRID_CELL_BUCKET );
	const uint dataSize = GRID_CELL_COUNT;

	const uint srcDataBaseID = dataOffset + prevBlockID * dataSize;
	const uint dstDataBaseID = dataOffset + newBlockID * dataSize;

	for ( uint cellID = 0; cellID < GRID_CELL_COUNT; ++cellID )
		streamBlockData[dstDataBaseID + cellID] = blockData[srcDataBaseID + cellID];
}

#define EXPORT_STREAM_SCATTER 1
#include "stream_scan_cs_impl.hlsli"

[ numthreads( HLSL_STREAM_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
	Scatter( Gid.x, GTid.x, DTid.x );
}
