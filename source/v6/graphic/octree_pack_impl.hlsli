#define HLSL

#include "capture_shared.h"

#if ONION == 1
#define OCTREE_LEAF OctreeLeafOnion
#else
#define OCTREE_LEAF OctreeLeaf
#endif

Buffer< uint > firstChildOffsets0					: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT0 );
Buffer< uint > firstChildOffsets1					: REGISTER_SRV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT1 );
StructuredBuffer< OCTREE_LEAF > octreeLeaves0		: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT0 );
StructuredBuffer< OCTREE_LEAF > octreeLeaves1		: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT1 );
StructuredBuffer< OCTREE_LEAF > octreeLeaves2		: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT2 );
StructuredBuffer< OCTREE_LEAF > octreeLeaves3		: REGISTER_SRV( HLSL_OCTREE_LEAF_SLOT3 );
StructuredBuffer< OctreeInfo > octreeInfo			: REGISTER_SRV( HLSL_OCTREE_INFO_SLOT );

RWBuffer< uint > blockPositions						: REGISTER_UAV( HLSL_BLOCK_POS_SLOT );
RWBuffer< uint > blockData							: REGISTER_UAV( HLSL_BLOCK_DATA_SLOT );
RWStructuredBuffer< BlockInfo > blockInfo			: REGISTER_UAV( HLSL_BLOCK_INFO_SLOT );

#define OCTREE_LEAF_IS_READONLY					1
#define OCTREE_FIRST_CHILD_OFFSET_IS_READONLY	1
#include "octree_helpers.hlsli"

[ numthreads( HLSL_OCTREE_THREAD_GROUP_SIZEX, HLSL_OCTREE_THREAD_GROUP_SIZEY, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint leafID = mad( DTid.y, HLSL_OCTREE_THREAD_MAX_COUNTX, DTid.x );
	if ( leafID >= octreeInfo[0].leafCount )
		return;

	const uint buckets[] =			{ 0, 4, 8, 16, 32, 64 };

	const uint blockDataSize[] =	buckets;
	const uint dataSizeInPreviousBucket = blockDataSize[c_octreeCurrentBucket];

	const uint posOffset = blockInfo[0].posOffsets[c_octreeCurrentBucket-1] + blockInfo[0].counts[c_octreeCurrentBucket-1];
	const uint dataOffset = blockInfo[0].dataOffsets[c_octreeCurrentBucket-1] + blockInfo[0].counts[c_octreeCurrentBucket-1] * dataSizeInPreviousBucket;

	if ( DTid.x == 0 && DTid.y == 0 )
	{
		blockInfo[0].posOffsets[c_octreeCurrentBucket] = posOffset;
		blockInfo[0].dataOffsets[c_octreeCurrentBucket] = dataOffset;

		if ( c_octreeCurrentBucket )
		{
			blockInfo[0].minNullLeafID = 0xFFFFFFFF;
			blockInfo[0].maxNullLeafID = 0;
			blockInfo[0].minOverLeafID = 0xFFFFFFFF;
			blockInfo[0].maxOverLeafID = 0;
		}
	}

	const OCTREE_LEAF octreeLeaf = ReadOctreeLeaf( leafID );

	if ( octreeLeaf.count32 == 0 )
	{
		InterlockedMin( blockInfo[0].minNullLeafID, leafID );
		blockInfo[0].maxNullLeafID = octreeInfo[0].leafCount;
	}

	uint3 coords;
#if ONION == 1
	coords.x = ((octreeLeaf.face3_x9_y9_z11 >> 18) & 0x07FC);
	coords.y = ((octreeLeaf.face3_x9_y9_z11 >>  9) & 0x07FC);
	coords.z = ((octreeLeaf.face3_x9_y9_z11 <<  2) & 0x1FFC);

	const uint face = (octreeLeaf.face3_x9_y9_z11 >> 29) & 7;
#else
	coords.x = ((octreeLeaf.mip4_none1_x9_y9_z9 >> 16) & 0x7FC);
	coords.y = ((octreeLeaf.mip4_none1_x9_y9_z9 >>  7) & 0x7FC);
	coords.z = ((octreeLeaf.mip4_none1_x9_y9_z9 <<  2) & 0x7FC);

	const uint mip = (octreeLeaf.mip4_none1_x9_y9_z9 >> 28) & 0xF;
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
			const uint firstChildOffset = ReadOctreeFirstChildOffset( nodeOffset ) & ~HLSL_NODE_CREATED;
			childOffset = firstChildOffset + cellOffset;
		}
		nodeOffset = childOffset;
	}

	uint cellRGBA[64];
	uint cellCount = 0;
	uint firstLeafID = 0xFFFFFFFF;

	const uint firstChildOffset = ReadOctreeFirstChildOffset( nodeOffset ) & ~HLSL_NODE_CREATED;

	const uint cellMinCount = buckets[c_octreeCurrentBucket]+1;
	const uint cellMaxCount = buckets[c_octreeCurrentBucket+1];

	for ( uint childID0 = 0; childID0 < 8; ++childID0 )
	{
		uint childNodeOffset0 = ReadOctreeFirstChildOffset( firstChildOffset + childID0 );
		if ( childNodeOffset0 )
		{
			childNodeOffset0 &= ~HLSL_NODE_CREATED;

			for ( uint childID1 = 0; childID1 < 8; ++childID1 )
			{
				uint childLeafID = ReadOctreeFirstChildOffset( childNodeOffset0 + childID1 );
				if ( childLeafID )
				{
					childLeafID &= ~HLSL_NODE_CREATED;

					firstLeafID = firstLeafID == 0xFFFFFFFF ? childLeafID : firstLeafID;

					[branch]
					if ( firstLeafID != leafID )
						return;

					const OCTREE_LEAF childOctreeLeaf = ReadOctreeLeaf( childLeafID );
					const uint sampleCount = childOctreeLeaf.count32;

					if ( sampleCount == 0 )
					{
						InterlockedMin( blockInfo[0].minNullLeafID, childLeafID );
						blockInfo[0].maxNullLeafID = octreeInfo[0].leafCount;
					}

					if ( childLeafID >= octreeInfo[0].leafCount )
					{
						InterlockedMin( blockInfo[0].minOverLeafID, childLeafID );
						blockInfo[0].maxOverLeafID = octreeInfo[0].leafCount;
					}

#if 1
					const uint r = childOctreeLeaf.r32 / sampleCount;
					const uint g = childOctreeLeaf.g32 / sampleCount;
					const uint b = childOctreeLeaf.b32 / sampleCount;
#else
					const uint3 leafColor = GetOctreeLeafDebugColor( childLeafID );
					const uint r = leafColor.r;
					const uint g = leafColor.g;
					const uint b = leafColor.b;
#endif
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

	InterlockedAdd( blockInfo[0].cellCounts[c_octreeCurrentBucket], cellCount );

	uint blockID;
	InterlockedAdd( blockInfo[0].counts[c_octreeCurrentBucket], 1, blockID );

	const uint dataSize = blockDataSize[c_octreeCurrentBucket+1];
	const uint dataBaseID = dataOffset + blockID * dataSize;
	const uint posBaseID = posOffset + blockID;

	const uint3 blockCoords = coords >> 2;
#if ONION == 1
	const uint blockPos = (blockCoords.z << 18) | (blockCoords.y << 9) | blockCoords.x;
	const uint packedBlockPos = (face << 29) | blockPos;
#else
	const uint gridMacroShift = c_octreeLevelCount - 2;
	const uint blockPos = (blockCoords.z << (gridMacroShift * 2)) | (blockCoords.y << gridMacroShift) | blockCoords.x;
	const uint packedBlockPos = (mip << 28) | blockPos;
#endif
	blockPositions[posBaseID] = packedBlockPos;

	for ( uint cellID = 0; cellID < cellMaxCount; ++cellID )
		blockData[dataBaseID+cellID] = cellID < cellCount ? cellRGBA[cellID] : HLSL_GRID_BLOCK_CELL_EMPTY;
}
