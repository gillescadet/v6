#define HLSL

#include "capture_shared.h"
#include "sample_pack.hlsli"

#if ONION == 1
#define OCTREE_LEAF OctreeLeafOnion
#else
#define OCTREE_LEAF OctreeLeaf
#endif

StructuredBuffer< Sample > samples					: REGISTER_SRV( HLSL_SAMPLE_SLOT );
StructuredBuffer< SampleInfo > sampleInfo			: REGISTER_SRV( HLSL_SAMPLE_INFO_SLOT );

RWBuffer< uint > sampleNodeOffsets 					: REGISTER_UAV( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT );
RWBuffer< uint > firstChildOffsets0					: REGISTER_UAV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT0 );
RWBuffer< uint > firstChildOffsets1					: REGISTER_UAV( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT1 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves0		: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT0 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves1		: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT1 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves2		: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT2 );
RWStructuredBuffer< OCTREE_LEAF > octreeLeaves3		: REGISTER_UAV( HLSL_OCTREE_LEAF_SLOT3 );
RWStructuredBuffer< OctreeInfo > octreeInfo 		: REGISTER_UAV( HLSL_OCTREE_INFO_SLOT );

#define OCTREE_LEAF_IS_READONLY					0
#define OCTREE_FIRST_CHILD_OFFSET_IS_READONLY	0
#include "octree_helpers.hlsli"

[ numthreads( HLSL_SAMPLE_THREAD_GROUP_SIZEX, HLSL_SAMPLE_THREAD_GROUP_SIZEY, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	InterlockedAdd( octreeInfo[0].sampleInputCount, 1 );

	const uint sampleID = mad( DTid.y, HLSL_SAMPLE_THREAD_MAX_COUNTX, DTid.x );
	if ( sampleID >= sampleInfo[0].count )
		return;

	InterlockedAdd( octreeInfo[0].samplePassedCount, 1 );

	uint3 coords;
	uint faceOrMip;
#if ONION
	SampleOnion_UnpackCoordsAndFace( samples[sampleID], coords, faceOrMip );
#else
	Sample_UnpackCoordsAndMip( samples[sampleID], coords, faceOrMip );
#endif
	
	// Node levels			: [ 0 .. c_octreeLevelCount-2 ]
	// Leaf level			: c_octreeLevelCount-1

	const uint3 cellCoords = coords >> (c_octreeLevelCount - c_octreeCurrentLevel - 1);
	const uint cellOffset = ((cellCoords.z&1)<<2) + ((cellCoords.y&1)<<1) + (cellCoords.x&1);

	uint childOffset;
	if ( c_octreeCurrentLevel == 0 )
	{		
#if ONION == 1
		childOffset = faceOrMip * 8 + cellOffset;
#else
		childOffset = faceOrMip * 8 + cellOffset;
#endif
	}
	else
	{
		const uint nodeOffset = sampleNodeOffsets[sampleID];
		const uint firstChildOffset = ReadOctreeFirstChildOffset( nodeOffset ) & ~HLSL_NODE_CREATED;
		childOffset = firstChildOffset + cellOffset;
	}
	
	sampleNodeOffsets[sampleID] = childOffset;

	const bool isNewNode = CreateOctreeFirstChildOffset( childOffset );
	[branch]
	if ( !isNewNode )
		return;

#if BUILD_INNER == 1
	uint newChildOffset;
	InterlockedAdd( octreeInfo[0].nodeCount, 8, newChildOffset );
	newChildOffset += HLSL_GRID_MAX_COUNT * 8; // root offset

	WriteOctreeFirstChildOffset( newChildOffset+0, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+1, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+2, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+3, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+4, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+5, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+6, 0 );
	WriteOctreeFirstChildOffset( newChildOffset+7, 0 );

	WriteOctreeFirstChildOffset( childOffset, HLSL_NODE_CREATED | newChildOffset );
#else
	uint newLeafID;
	InterlockedAdd( octreeInfo[0].leafCount, 1, newLeafID );

	InitOctreeLeaf( newLeafID, coords, faceOrMip );

	WriteOctreeFirstChildOffset( childOffset, HLSL_NODE_CREATED | newLeafID );
#endif
}
