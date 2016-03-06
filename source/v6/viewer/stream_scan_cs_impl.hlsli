#define INTERLEAVE_S( S, SHIFT, OFFSET )				(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )						INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )						INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )						INTERLEAVE_S( Z, SHIFT, 2 )

uint ComputeKeyFromPackedBlockPos( uint packedBlockPos )
{
	const uint mip = packedBlockPos >> 28;
	const uint blockPos = packedBlockPos & 0x0FFFFFFF;
	const uint x = (blockPos >> 0)						 & HLSL_GRID_MACRO_MASK;
	const uint y = (blockPos >> HLSL_GRID_MACRO_SHIFT)	 & HLSL_GRID_MACRO_MASK;
	const uint z = (blockPos >> HLSL_GRID_MACRO_2XSHIFT) & HLSL_GRID_MACRO_MASK;

	uint key = mip << HLSL_GRID_MACRO_3XSHIFT;
	for ( uint shift = 0; shift < HLSL_GRID_MACRO_SHIFT; ++shift )
	{
		key |= INTERLEAVE_X( x, shift );
		key |= INTERLEAVE_Y( y, shift );
		key |= INTERLEAVE_Z( z, shift );
	}

	return key;
}

#if EXPORT_STREAM_SET_BIT == 1

void SetBit( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < block_count( GRID_CELL_BUCKET ) )
	{
		const uint posOffset = block_posOffset( GRID_CELL_BUCKET );
		const uint blockPosID = posOffset + blockID;

		const uint key = ComputeKeyFromPackedBlockPos( blockPositions[blockPosID] );

		const uint chunk = key >> 5;
		const uint bit = key & 0x1F;

		InterlockedOr( streamBits[chunk], 1 << bit );

		const uint keyGroup = chunk >> HLSL_STREAM_THREAD_GROUP_SHIFT;

		const uint chunkGroup = keyGroup >> 5;
		const uint bitGroup = keyGroup & 0x1F;

		InterlockedOr( streamGroupBits[chunkGroup], 1 << bitGroup );
	}
}

#endif // #if EXPORT_STREAM_SET_BIT == 1

#if EXPORT_STREAM_PREFIX_SUM == 1

groupshared uint g_counts[HLSL_STREAM_THREAD_GROUP_SIZE];

void PrefixSum( uint groupID, uint threadGroupID, uint threadID )
{
	if ( c_streamCurrentOffset == 0 )
	{
		const uint chunkGroup = groupID >> 5;
		const uint bitGroup = groupID & 0x1F;

		if ( (streamGroupBits[chunkGroup] & (1 << bitGroup)) == 0 )
			return;

		const uint count = countbits( streamBits[threadID] );
		g_counts[threadGroupID] = count;
	}
	else
	{
		g_counts[threadGroupID] = streamCounts[c_streamCurrentOffset + threadID];
	}

	const uint stepCount = firstbithigh( HLSL_STREAM_THREAD_GROUP_SIZE );

	AllMemoryBarrierWithGroupSync();

	{
		for ( uint step = 0; step < stepCount; ++step )
		{
			const uint offset = 1 << step;
			const uint posMask = (offset << 1) - 1;
			if ( (threadGroupID & posMask) == posMask )
			{
				const int otherGroupID = threadGroupID-offset;
				V6_ASSERT( otherGroupID >= 0 );
				g_counts[threadGroupID] += g_counts[otherGroupID];
			}

			AllMemoryBarrierWithGroupSync();
		}

		if ( threadGroupID == HLSL_STREAM_THREAD_GROUP_SIZE-1 )
		{
			if ( c_streamLowerOffset != 0 )
				streamCounts[c_streamLowerOffset + groupID] = g_counts[HLSL_STREAM_THREAD_GROUP_SIZE-1];
			g_counts[HLSL_STREAM_THREAD_GROUP_SIZE-1] = 0;
		}
	}

	AllMemoryBarrierWithGroupSync();

	{
		for ( int step = stepCount-1; step >= 0; --step )
		{
			const uint offset = 1 << step;
			const uint posMask = (offset << 1) - 1;
			if ( (threadGroupID & posMask) == posMask )
			{
				const int leftGroupID = threadGroupID-offset;
				V6_ASSERT( leftGroupID >= 0 );
				const uint sum = g_counts[leftGroupID] + g_counts[threadGroupID];
				g_counts[leftGroupID] = g_counts[threadGroupID];
				g_counts[threadGroupID] = sum;
			}

			AllMemoryBarrierWithGroupSync();
		}
	}

	streamCounts[c_streamCurrentOffset + threadID] = g_counts[threadGroupID];
}

#endif // #if EXPORT_STREAM_PREFIX_SUM == 1

#if EXPORT_STREAM_SUMMARIZE == 1

void Summarize( uint groupID, uint threadGroupID, uint threadID )
{
	const uint chunkGroup = groupID >> 5;
	const uint bitGroup = groupID & 0x1F;

	if ( (streamGroupBits[chunkGroup] & (1 << bitGroup)) == 0 )
		return;

	uint address = 0;

	uint elementCount = HLSL_STREAM_SIZE;
	uint offsetID = threadID;
	uint streamOffset = 0;
	for ( uint layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT, offsetID >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
	{
		address += streamCounts[streamOffset + offsetID];
		streamOffset += elementCount;
	}

	streamAddresses[threadID] = address;
}

#endif // #if EXPORT_STREAM_SUMMARIZE == 1

#if EXPORT_STREAM_SCATTER == 1

void Scatter( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < block_count( GRID_CELL_BUCKET ) )
	{
		const uint posOffset = block_posOffset( GRID_CELL_BUCKET );
		const uint blockPosID = posOffset + blockID;

		const uint packedBlockPos = blockPositions[blockPosID];
		const uint key = ComputeKeyFromPackedBlockPos( packedBlockPos );

		const uint chunk = key >> 5;
		const uint bit = key & 0x1F;
		const uint prevBitMask = (1 << bit) - 1;

		const uint prevBits = streamBits[chunk] & prevBitMask;
		const uint rank = countbits( prevBits );

		const uint finalAddress = streamAddresses[chunk] + rank;

		ScatterBlock( packedBlockPos, blockID, finalAddress );
	}
}

#endif // #if EXPORT_STREAM_SCATTER == 1