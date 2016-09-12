#define HLSL
#include "../graphic/capture_shared.h"

Buffer< uint > firstChildOffsets					: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT );
#if ONION == 1
StructuredBuffer< OctreeLeafOnion > octreeLeaves	: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT );
#else
StructuredBuffer< OctreeLeaf > octreeLeaves			: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT );
#endif
Buffer< uint > octreeIndirectArgs 					: REGISTER_SRV( HLSL_OCTREE_INDIRECT_ARGS_SLOT );

RWBuffer< uint > blockPositions						: REGISTER_UAV( HLSL_BLOCK_POS_SLOT );
RWBuffer< uint > blockData							: REGISTER_UAV( HLSL_BLOCK_DATA_SLOT );
RWBuffer< uint > blockIndirectArgs 					: REGISTER_UAV( HLSL_BLOCK_INDIRECT_ARGS_SLOT );

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZE, 1, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint leafID = DTid.x;
	if ( leafID >= octree_leafCount )
		return;
		
	const uint buckets[] =			{ 0, 4, 8, 16, 32, 64 };

	const uint blockDataSize[] =	buckets;
	const uint dataSizeInPreviousBucket = blockDataSize[c_octreeCurrentBucket];

	const uint posOffset = block_posOffset( c_octreeCurrentBucket-1 ) + block_count( c_octreeCurrentBucket-1 );
	const uint dataOffset = block_dataOffset( c_octreeCurrentBucket-1 ) + block_count( c_octreeCurrentBucket-1 ) * dataSizeInPreviousBucket;
		
	if ( DTid.x == 0 )
	{
		block_posOffset( c_octreeCurrentBucket ) = posOffset;
		block_dataOffset( c_octreeCurrentBucket ) = dataOffset;
		block_groupCountY( c_octreeCurrentBucket ) = 1;
		block_groupCountZ( c_octreeCurrentBucket ) = 1;
	}

	uint3 coords;
#if ONION == 1
	coords.x = ((octreeLeaves[leafID].face3_x9_y9_z11 >> 18) & 0x07FC) | ((octreeLeaves[leafID].done1_x2y2z2_count25 >> 29) & 0x3);
	coords.y = ((octreeLeaves[leafID].face3_x9_y9_z11 >>  9) & 0x07FC) | ((octreeLeaves[leafID].done1_x2y2z2_count25 >> 27) & 0x3);
	coords.z = ((octreeLeaves[leafID].face3_x9_y9_z11 <<  2) & 0x1FFC) | ((octreeLeaves[leafID].done1_x2y2z2_count25 >> 25) & 0x3);

	const uint face = (octreeLeaves[leafID].face3_x9_y9_z11 >> 29) & 7;
#else
	coords.x = ((octreeLeaves[leafID].mip4_none1_x9_y9_z9 >> 16) & 0x7FC) | ((octreeLeaves[leafID].done1_x2y2z2_count25 >> 29) & 0x3);
	coords.y = ((octreeLeaves[leafID].mip4_none1_x9_y9_z9 >>  7) & 0x7FC) | ((octreeLeaves[leafID].done1_x2y2z2_count25 >> 27) & 0x3);
	coords.z = ((octreeLeaves[leafID].mip4_none1_x9_y9_z9 <<  2) & 0x7FC) | ((octreeLeaves[leafID].done1_x2y2z2_count25 >> 25) & 0x3);

	const uint mip = (octreeLeaves[leafID].mip4_none1_x9_y9_z9 >> 28) & 0xF;
#endif

	const uint packLevel = c_octreeLevelCount - 3;

	uint nodeOffset;

	for ( uint level = 0; level <= packLevel; ++level )
	{		
		const uint3 cellCoords = coords >> (c_octreeLevelCount - level - 1);
		const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);

		int childOffset;
		if ( level == 0 )
		{
#if ONION == 1
			childOffset = face * 8 + cellOffset;
#else
			childOffset = mip * 8 + cellOffset;
#endif
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

	const uint cellMinCount = buckets[c_octreeCurrentBucket]+1;
	const uint cellMaxCount = buckets[c_octreeCurrentBucket+1];

	for ( uint childID0 = 0; childID0 < 8; ++childID0 )
	{
		uint childNodeOffset0 = firstChildOffsets[firstChildOffset+childID0];
		if ( childNodeOffset0 )
		{
			childNodeOffset0 &= ~HLSL_NODE_CREATED;

			for ( uint childID1 = 0; childID1 < 8; ++childID1 )
			{
				uint childLeafID = firstChildOffsets[childNodeOffset0+childID1];
				if ( childLeafID )
				{
					childLeafID &= ~HLSL_NODE_CREATED;

					firstLeafID = firstLeafID == 0xFFFFFFFF ? childLeafID  : firstLeafID;

					[branch]
					if ( firstLeafID != leafID )
						return;

					const uint sampleCount = octreeLeaves[childLeafID].done1_x2y2z2_count25 & 0xFFFFFF;
					const uint r = octreeLeaves[childLeafID].r32 / sampleCount;
					const uint g = octreeLeaves[childLeafID].g32 / sampleCount;
					const uint b = octreeLeaves[childLeafID].b32 / sampleCount;
					const uint cellPos = ((childID0&4)<<3) | ((childID1&4)<<2) | ((childID0&2)<<2) | ((childID1&2)<<1) | ((childID0&1)<<1) | ((childID1&1)<<0);
					cellRGBA[cellCount] = (r << 24) | (g << 16) | (b << 8) | cellPos;

					++cellCount;

					[branch]
					if ( cellCount > cellMaxCount )
						return;
				}
			}
		}
	}	
	
	[branch]
	if ( cellCount < cellMinCount )
		return;

	InterlockedAdd( block_cellCount( c_octreeCurrentBucket ), cellCount );

	uint blockID;
	InterlockedAdd( block_count( c_octreeCurrentBucket ), 1, blockID );
		
	const uint dataSize = blockDataSize[c_octreeCurrentBucket+1];
	const uint dataBaseID = dataOffset + blockID * dataSize;
	const uint posBaseID = posOffset + blockID;	

	const uint3 blockCoords = coords >> 2;
#if ONION == 1
	const uint blockPos = (blockCoords.z << 18) | (blockCoords.y << 9) | blockCoords.x;
	const uint packedBlockPos = blockPositions[posBaseID] = (face << 29) | blockPos;
#else
	const uint gridMacroShift = c_octreeLevelCount - 2;
	const uint blockPos = (blockCoords.z << (gridMacroShift * 2)) | (blockCoords.y << gridMacroShift) | blockCoords.x;
	const uint packedBlockPos = blockPositions[posBaseID] = (mip << 28) | blockPos;
#endif
	blockPositions[posBaseID] = packedBlockPos;

	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
		blockData[dataBaseID+cellID] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;

	const uint blockCount = blockID + 1;
	const uint groupCount = HLSL_GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );
	InterlockedMax( block_groupCountX( c_octreeCurrentBucket ), groupCount );
}
