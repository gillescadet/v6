#define HLSL
#include "common_shared.h"

Buffer< uint > firstChildOffsets				: register( HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV );
StructuredBuffer< OctreeLeaf > octreeLeaves		: register( HLSL_OCTREE_LEAF_SRV );
Buffer< uint > octreeIndirectArgs 				: register( HLSL_OCTREE_INDIRECT_ARGS_SRV );

RWBuffer< uint > packedColors					: register( HLSL_PACKED_COLOR_UAV );
RWBuffer< uint > packedIndirectArgs 			: register( HLSL_PACKED_INDIRECT_ARGS_UAV );

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint leafID = DTid.x;
	if ( leafID >= octree_leafCount( c_octreeCurrentMip ) )
		return;
	
	const uint blockOffset = packed_blockSum( c_octreeCurrentMip, c_octreeCurrentBucket-1 ) + packed_blockCount( c_octreeCurrentMip, c_octreeCurrentBucket-1 );

	if ( DTid.x == 0 )
	{
		packed_blockSum( c_octreeCurrentMip, c_octreeCurrentBucket ) = blockOffset;
		packed_vertexCountPerInstance( c_octreeCurrentMip, c_octreeCurrentBucket ) = 1;
		packed_renderInstanceLocation( c_octreeCurrentMip, c_octreeCurrentBucket ) = blockOffset;
	}

	leafID += octree_leafSum( c_octreeCurrentMip );

	uint3 coords;
	coords.x = ((octreeLeaves[leafID].x8_r >> 20) & ~0xF) | ((octreeLeaves[leafID].x4y4z4_count >> 28) & 0xF);
	coords.y = ((octreeLeaves[leafID].y8_g >> 20) & ~0xF) | ((octreeLeaves[leafID].x4y4z4_count >> 24) & 0xF);
	coords.z = ((octreeLeaves[leafID].z8_b >> 20) & ~0xF) | ((octreeLeaves[leafID].x4y4z4_count >> 20) & 0xF);

	const uint packLevel = HLSL_GRID_SHIFT-3;

	uint nodeOffset;

	[unroll]
	for ( uint level = 0; level <= packLevel; ++level )
	{		
		const uint3 cellCoords = coords >> (HLSL_GRID_SHIFT-level-1);
		const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);

		int childOffset;
		if ( level == 0 )
		{
			childOffset = c_octreeCurrentMip * 8 + cellOffset;
		}
		else
		{
			const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;
			childOffset = firstChildOffset + cellOffset;			
		}				
		nodeOffset = childOffset;		
	}

	uint cellRGBA[64];
	uint cellCount = 0;
	uint firstLeafID = 0xFFFFFFFF;

	const uint firstChildOffset = firstChildOffsets[nodeOffset] & ~HLSL_NODE_CREATED;

	[unroll]
	for ( uint childID0 = 0; childID0 < 8; ++childID0 )
	{
		uint childNodeOffset0 = firstChildOffsets[firstChildOffset+childID0];
		if ( childNodeOffset0 )
		{
			childNodeOffset0 &= ~HLSL_NODE_CREATED;

			[unroll]
			for ( uint childID1 = 0; childID1 < 8; ++childID1 )
			{
				uint childLeafID = firstChildOffsets[childNodeOffset0+childID1];
				if ( childLeafID )
				{
					childLeafID &= ~HLSL_NODE_CREATED;

					firstLeafID = min( firstLeafID, childLeafID );

					const uint sampleCount = octreeLeaves[childLeafID].x4y4z4_count & 0xFFFF;
					const uint r = (octreeLeaves[childLeafID].x8_r & 0x00FFFFFF) / sampleCount;
					const uint g = (octreeLeaves[childLeafID].y8_g & 0x00FFFFFF) / sampleCount;
					const uint b = (octreeLeaves[childLeafID].z8_b & 0x00FFFFFF) / sampleCount;
					const uint cellPos = ((childID0&4)<<3) | ((childID1&4)<<2) | ((childID0&2)<<2) | ((childID1&2)<<1) | ((childID0&1)<<1) | ((childID1&1)<<0);
					cellRGBA[cellCount] = (r << 24) | (g << 16) | (b << 8) | cellPos;
					++cellCount;
				}
			}
		}
	}

	const uint buckets[] = { 0, 4, 8, 16, 32, 64 };
	const uint cellMinCount = buckets[c_octreeCurrentBucket]+1;
	const uint cellMaxCount = buckets[c_octreeCurrentBucket+1];	

	[branch]
	if ( firstLeafID != leafID || cellCount < cellMinCount || cellCount > cellMaxCount )
		return;

	InterlockedAdd( packed_cellCount( c_octreeCurrentMip, c_octreeCurrentBucket ), cellCount );
			
	uint blockID;
	InterlockedAdd( packed_blockCount( c_octreeCurrentMip, c_octreeCurrentBucket ), 1, blockID );

	const uint packedCount = 1 + cellCount;
	
	const uint packedBaseID = (blockOffset + blockID) * packedCount;	
	const uint3 blockCoords = coords >> HLSL_GRID_BLOCK_SHIFT;
	const uint blockPos = (blockCoords.z << HLSL_GRID_MACRO_2XSHIFT) | (blockCoords.y << HLSL_GRID_MACRO_SHIFT) | blockCoords.x;
	packedColors[packedBaseID] = blockPos;	

	[unroll]
	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
		packedColors[packedBaseID + cellID + 1] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;

	const uint blockCount = blockID + 1;
	InterlockedMax( packed_renderInstanceCount( c_octreeCurrentMip, c_octreeCurrentBucket ), blockCount * cellMaxCount );
}
