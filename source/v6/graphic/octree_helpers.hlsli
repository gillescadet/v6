/*V6*/

#if OCTREE_LEAF_IS_READONLY == 0

void InitOctreeLeaf( uint leafID, uint3 coords, uint faceOrMip )
{
	OCTREE_LEAF leaf;
#if ONION == 1
	leaf.face3_x9_y9_z11 = (faceOrMip << 29) | ((coords.x & ~3) << 18) | ((coords.y & ~3) << 9) | (coords.z >> 2);
#else
	leaf.mip4_none1_x9_y9_z9 = (faceOrMip << 28) | ((coords.x & ~3) << 16) | ((coords.y & ~3) << 7) | (coords.z >> 2);
#endif

	leaf.count32 = 0;
	leaf.r32 = 0;
	leaf.g32 = 0;
	leaf.b32 = 0;

	const uint page = leafID >> HLSL_OCTREE_LEAF_SHIFT;
	const uint offset = leafID & HLSL_OCTREE_LEAF_MOD;

	switch( page )
	{
	case 0:
		octreeLeaves0[offset] = leaf;
		break;
	case 1:
		octreeLeaves1[offset] = leaf;
		break;
	case 2:
		octreeLeaves2[offset] = leaf;
		break;
	case 3:
		octreeLeaves3[offset] = leaf;
		break;
	}
}

void UpdateOctreeLeaf( uint leafID, uint3 color )
{
	const uint page = leafID >> HLSL_OCTREE_LEAF_SHIFT;
	const uint offset = leafID & HLSL_OCTREE_LEAF_MOD;

	switch( page )
	{
	case 0:
		InterlockedAdd( octreeLeaves0[offset].count32, 1 );
		InterlockedAdd( octreeLeaves0[offset].r32, color.r );
		InterlockedAdd( octreeLeaves0[offset].g32, color.g );
		InterlockedAdd( octreeLeaves0[offset].b32, color.b );
		break;
	case 1:
		InterlockedAdd( octreeLeaves1[offset].count32, 1 );
		InterlockedAdd( octreeLeaves1[offset].r32, color.r );
		InterlockedAdd( octreeLeaves1[offset].g32, color.g );
		InterlockedAdd( octreeLeaves1[offset].b32, color.b );
		break;
	case 2:
		InterlockedAdd( octreeLeaves2[offset].count32, 1 );
		InterlockedAdd( octreeLeaves2[offset].r32, color.r );
		InterlockedAdd( octreeLeaves2[offset].g32, color.g );
		InterlockedAdd( octreeLeaves2[offset].b32, color.b );
		break;
	case 3:
		InterlockedAdd( octreeLeaves3[offset].count32, 1 );
		InterlockedAdd( octreeLeaves3[offset].r32, color.r );
		InterlockedAdd( octreeLeaves3[offset].g32, color.g );
		InterlockedAdd( octreeLeaves3[offset].b32, color.b );
		break;
	}
}

#endif // #if OCTREE_LEAF_IS_READONLY == 0

uint3 GetOctreeLeafDebugColor( uint leafID )
{
	const uint page = leafID >> HLSL_OCTREE_LEAF_SHIFT;

	if ( page > 4 )
		return uint3( 255, 255, 255 );

	const uint3 colors[4] = { uint3( 255, 0, 0 ), uint3( 0, 255, 0 ), uint3( 0, 0, 255 ), uint3( 255, 255, 0 ) };
	return colors[page];
}

OCTREE_LEAF ReadOctreeLeaf( uint leafID )
{
	const uint page = leafID >> HLSL_OCTREE_LEAF_SHIFT;
	const uint offset = leafID & HLSL_OCTREE_LEAF_MOD;

	OCTREE_LEAF leaf = (OCTREE_LEAF)0;

	switch( page )
	{
	case 0:
		leaf = octreeLeaves0[offset];
		break;
	case 1:
		leaf = octreeLeaves1[offset];
		break;
	case 2:
		leaf = octreeLeaves2[offset];
		break;
	case 3:
		leaf = octreeLeaves3[offset];
		break;
	}

	return leaf;
}

uint ReadOctreeFirstChildOffset( uint nodeOffset )
{
	const uint page = nodeOffset >> HLSL_OCTREE_FIRST_CHILD_OFFSET_SHIFT;
	const uint offset = nodeOffset & HLSL_OCTREE_FIRST_CHILD_OFFSET_MOD;

	uint childOffset = 0;

	switch( page )
	{
	case 0:
		childOffset = firstChildOffsets0[offset];
		break;
	case 1:
		childOffset = firstChildOffsets1[offset];
		break;
	}

	return childOffset;
}

#if OCTREE_FIRST_CHILD_OFFSET_IS_READONLY == 0

void WriteOctreeFirstChildOffset( uint nodeOffset, uint childOffset )
{
	const uint page = nodeOffset >> HLSL_OCTREE_FIRST_CHILD_OFFSET_SHIFT;
	const uint offset = nodeOffset & HLSL_OCTREE_FIRST_CHILD_OFFSET_MOD;

	switch( page )
	{
	case 0:
		firstChildOffsets0[offset] = childOffset;
		break;
	case 1:
		firstChildOffsets1[offset] = childOffset;
		break;
	}
}

bool CreateOctreeFirstChildOffset( uint nodeOffset )
{
	const uint page = nodeOffset >> HLSL_OCTREE_FIRST_CHILD_OFFSET_SHIFT;
	const uint offset = nodeOffset & HLSL_OCTREE_FIRST_CHILD_OFFSET_MOD;

	uint prevFirstChildOffset = 0;

	switch( page )
	{
	case 0:
		InterlockedCompareExchange( firstChildOffsets0[offset], 0, HLSL_NODE_CREATED, prevFirstChildOffset );
		break;
	case 1:
		InterlockedCompareExchange( firstChildOffsets1[offset], 0, HLSL_NODE_CREATED, prevFirstChildOffset );
		break;
	}

	return prevFirstChildOffset == 0;
}

#endif // #if OCTREE_FIRST_CHILD_OFFSET_IS_READONLY == 0
