/*V6*/

#pragma comment( lib, "core.lib" )

#include <v6/core/common.h>

#include <v6/core/codec.h>
#include <v6/core/compute.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/vec3i.h>

#include <v6/viewer/common_shared.h>

#define V6_MERGE_VERBOSE				0

#define V6_MERGE_SHARED_FRAME_MAX_COUNT	256

BEGIN_V6_HLSL_NAMESPACE

struct MergeContext
{
	uint blockPosOffset;
	uint blockDataOffset;
};

uint* blockPositions = nullptr;
uint* refBlockPositions = nullptr;
uint* blockData = nullptr;
uint* refBlockData = nullptr;
uint* blockCounters = nullptr;
uint* bits = nullptr;
uint* refBits = nullptr;
uint* commonBits = nullptr;
uint* groupBits = nullptr;
uint* counts = nullptr;
uint* addresses = nullptr;
uint* outBlockPositions = nullptr;
uint* outBlockData = nullptr;
MergeContext* mergeContext = nullptr;

uint c_mergeBlockCount;
uint c_mergeBlockPosOffset;
uint c_mergeBlockDataOffset;
uint c_mergeOutBlockPosOffset;
uint c_mergeOutBlockDataOffset;
uint c_mergeCellPerBucketCount;
int3 c_mergeRefBlockPosTranslation;
int3 c_mergeBlockPosTranslation;

uint c_scatterBlockDataOffset;
uint c_scatterBlockCellCount;

uint c_mergeCurrentOffset;
uint c_mergeLowerOffset;

uint c_mergeMip;
uint c_mergeCounter;

#define INTERLEAVE_S( S, SHIFT, OFFSET )				(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )						INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )						INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )						INTERLEAVE_S( Z, SHIFT, 2 )

uint ComputeKeyFromBlockPos( uint blockPos, int3 blockPosTransation )
{
	const uint x = ((blockPos >> 0)							& HLSL_GRID_MACRO_MASK) + blockPosTransation.x;
	const uint y = ((blockPos >> HLSL_GRID_MACRO_SHIFT)		& HLSL_GRID_MACRO_MASK) + blockPosTransation.y;
	const uint z = ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK) + blockPosTransation.z;

	uint key = 0;
	for ( uint shift = 0; shift < HLSL_GRID_MACRO_SHIFT; ++shift )
	{
		key |= INTERLEAVE_X( x, shift );
		key |= INTERLEAVE_Y( y, shift );
		key |= INTERLEAVE_Z( z, shift );
	}

	return key;
}

uint ComputeKeyFromPackedBlockPos( uint packedBlockPos )
{
	const uint mip = packedBlockPos >> 28;
	const uint blockPos = packedBlockPos & 0x0FFFFFFF;

	uint key = mip << HLSL_GRID_MACRO_3XSHIFT;
	key |= ComputeKeyFromBlockPos( blockPos, int3( 0, 0, 0 ) );
	return key;
}

uint Sort_GetKey( uint blockPosID, uint packedBlockPos )
{	
	const uint mip = packedBlockPos >> 28;

	if ( blockCounters[blockPosID] == c_mergeCounter )
	{
		const uint key = ComputeKeyFromPackedBlockPos( packedBlockPos );
		return key;
	}

	return (uint)-1;
}

void Sort_ScatterBlock( uint packedBlockPos, uint prevBlockID, uint newBlockID )
{
	const uint blockPosID = c_mergeOutBlockPosOffset + newBlockID;
	outBlockPositions[blockPosID] = packedBlockPos;

	const uint dataSize = c_mergeCellPerBucketCount;
	const uint srcDataBaseID = c_mergeBlockDataOffset + prevBlockID * dataSize;
	const uint dstDataBaseID = c_mergeOutBlockDataOffset + newBlockID * dataSize;

	for ( uint cellID = 0; cellID < dataSize; ++cellID )
		outBlockData[dstDataBaseID + cellID] = blockData[srcDataBaseID + cellID];
}

groupshared uint* g_counts;

uint (*s_Compaction_GetKey)( uint blockPosID, uint packedBlockPos ) = nullptr;

#if V6_CPP == 1
template < bool USE_GROUP_BITS >
#endif // #if V6_CPP == 1
void Compaction_SetBit( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < c_mergeBlockCount )
	{
		const uint blockPosID = c_mergeBlockPosOffset + blockID;

		const uint packedBlockPos = blockPositions[blockPosID];
		const uint key = s_Compaction_GetKey( blockPosID, packedBlockPos );

		if ( key != (uint)-1 )
		{
			const uint chunk = key >> 5;
			const uint bit = key & 0x1F;

			uint prevValue;
			InterlockedOr( bits[chunk], 1 << bit, prevValue );
			V6_ASSERT( (prevValue & (1 << bit)) == 0)

#if USE_GROUP_BITS == 1
			const uint keyGroup = chunk >> HLSL_STREAM_THREAD_GROUP_SHIFT;

			const uint chunkGroup = keyGroup >> 5;
			const uint bitGroup = keyGroup & 0x1F;

			InterlockedOr( groupBits[chunkGroup], 1 << bitGroup );
#endif // #if USE_GROUP_BITS == 1
		}
	}
}

#if V6_CPP == 1
template < uint THREAD_GROUP_SIZE >
#endif
void Compaction_PrefixSum( uint groupID, uint threadGroupID, uint threadID )
{
	if ( c_mergeCurrentOffset == 0 )
	{
		const uint chunkGroup = groupID >> 5;
		const uint bitGroup = groupID & 0x1F;

		if ( (groupBits[chunkGroup] & (1 << bitGroup)) == 0 )
			return;

		const uint count = countbits( bits[threadID] );
		g_counts[threadGroupID] = count;
	}
	else
	{
		g_counts[threadGroupID] = counts[c_mergeCurrentOffset + threadID];
	}

	const uint stepCount = firstbithigh( THREAD_GROUP_SIZE );

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

		if ( threadGroupID == THREAD_GROUP_SIZE-1 )
		{
			counts[c_mergeLowerOffset + groupID] = g_counts[THREAD_GROUP_SIZE-1];
			g_counts[THREAD_GROUP_SIZE-1] = 0;
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

	counts[c_mergeCurrentOffset + threadID] = g_counts[threadGroupID];
}

#if V6_CPP == 1
template < uint ELEMENT_COUNT, uint LAYER_COUNT, uint THREAD_GROUP_SHIFT  >
#endif
void Compaction_Summarize( uint groupID, uint threadGroupID, uint threadID )
{
	const uint chunkGroup = groupID >> 5;
	const uint bitGroup = groupID & 0x1F;

	if ( (groupBits[chunkGroup] & (1 << bitGroup)) == 0 )
		return;

	uint address = 0;

	uint elementCount = ELEMENT_COUNT;
	uint offsetID = threadID;
	uint streamOffset = 0;
	for ( uint layer = 0; layer < LAYER_COUNT; ++layer, elementCount >>= THREAD_GROUP_SHIFT, offsetID >>= THREAD_GROUP_SHIFT )
	{
		address += counts[streamOffset + offsetID];
		streamOffset += elementCount;
	}

	addresses[threadID] = address;
}

void (*s_ScatterBlock)( uint packedBlockPos, uint prevBlockID, uint newBlockID ) = nullptr;

void Compaction_Scatter( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < c_mergeBlockCount )
	{
		const uint blockPosID = c_mergeBlockPosOffset + blockID;

		const uint packedBlockPos = blockPositions[blockPosID];
		const uint key = s_Compaction_GetKey( blockPosID, packedBlockPos );

		if ( key != (uint)-1 )
		{
			const uint chunk = key >> 5;
			const uint bit = key & 0x1F;
			const uint bitMask = 1 << bit;
	
			if ( bits[chunk] & bitMask )
			{
				const uint prevBitMask = bitMask - 1;
				const uint prevBits = bits[chunk] & prevBitMask;
				const uint rank = countbits( prevBits );

				const uint finalAddress = addresses[chunk] + rank;

				s_ScatterBlock( packedBlockPos, blockID, finalAddress );
			}
		}
	}
}

uint Compare_GetKey( uint blockPosID, uint packedBlockPos )
{	
	const uint mip = packedBlockPos >> 28;

	if ( mip == c_mergeMip && blockCounters[blockPosID] >= c_mergeCounter )
	{
		const uint blockPos = packedBlockPos & 0x0FFFFFFF;
		const uint key = ComputeKeyFromBlockPos( blockPos, c_mergeBlockPosTranslation );

		return key;
	}

	return (uint)-1;
}

void Compare_AndBit( uint groupID, uint threadGroupID, uint threadID )
{
	bits[threadID] &= refBits[threadID];

	if ( bits[threadID] )
	{
		const uint chunkGroup = groupID >> 5;
		const uint bitGroup = groupID & 0x1F;

		InterlockedOr( groupBits[chunkGroup], 1 << bitGroup );
	}
}

void Compare_ScatterBlock( uint packedBlockPos, uint prevBlockID, uint newBlockID )
{
	outBlockPositions[newBlockID] = packedBlockPos;

	const uint dataOffset = c_mergeBlockDataOffset;
	const uint dataSize = c_mergeCellPerBucketCount;

	const uint srcDataBaseID = dataOffset + prevBlockID * dataSize;
	const uint dstDataBaseID = newBlockID * dataSize;

	for ( uint cellID = 0; cellID < dataSize; ++cellID )
		outBlockData[dstDataBaseID + cellID] = blockData[srcDataBaseID + cellID];
}

void Compare_UnsetBit( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < c_mergeBlockCount )
	{
		const uint blockPosID = blockID;
		const uint blockDataOffset = blockID * c_mergeCellPerBucketCount;

		const uint refPackedBlockPos = refBlockPositions[blockPosID];
		const uint refBlockPos = refPackedBlockPos & 0x0FFFFFFF;
		const uint refKey = ComputeKeyFromBlockPos( refBlockPos, c_mergeRefBlockPosTranslation );

		const uint newPackedBlockPos = blockPositions[blockPosID];
		const uint newBlockPos = newPackedBlockPos & 0x0FFFFFFF;
		const uint key = ComputeKeyFromBlockPos( newBlockPos, c_mergeBlockPosTranslation );

		V6_ASSERT( refKey == key );

		bool different = false;
		for ( uint cellID = 0; cellID < c_mergeCellPerBucketCount; ++cellID )
		{
			const uint blockDataID = blockDataOffset + cellID;
			if ( refBlockData[blockDataID] != blockData[blockDataID] )
			{
				different = true;
				break;
			}
		}

		if ( different )
		{
			const uint chunk = key >> 5;
			const uint bit = key & 0x1F;

			InterlockedAnd( bits[chunk], ~(1 << bit) );
		}
	}
}

void Compare_IncCounter( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < c_mergeBlockCount )
	{
		const uint blockPosID = c_mergeBlockPosOffset + blockID;

		const uint packedBlockPos = blockPositions[blockPosID];
		const uint mip = packedBlockPos >> 28;

		if ( mip == c_mergeMip )
		{
			const uint blockPos = packedBlockPos & 0x0FFFFFFF;
			const uint key = ComputeKeyFromBlockPos( blockPos, c_mergeBlockPosTranslation );

			const uint chunk = key >> 5;
			const uint bit = key & 0x1F;

			if ( bits[chunk] & (1 << bit) )
				++blockCounters[blockPosID];
		}
	}
}

void TrimShared( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < c_mergeBlockCount )
	{
		const uint srcBlockPosID = c_mergeBlockPosOffset + blockID;
		
		if ( blockCounters[srcBlockPosID] == 0 )
		{
			const uint srcBlockDataID = c_mergeBlockDataOffset + blockID * c_mergeCellPerBucketCount;

			uint dstBlockPosID;
			uint dstBlockDataID;
			InterlockedAdd( mergeContext[0].blockPosOffset, 1, dstBlockPosID );
			InterlockedAdd( mergeContext[0].blockDataOffset, c_mergeCellPerBucketCount, dstBlockDataID );
		
			const uint packedBlockPos = blockPositions[srcBlockPosID];
			outBlockPositions[dstBlockPosID] = packedBlockPos;
			for ( uint cellID = 0; cellID < c_mergeCellPerBucketCount; ++cellID )
				outBlockData[dstBlockDataID + cellID] = blockData[srcBlockDataID + cellID];
		}
	}
}

END_V6_HLSL_NAMESPACE

using namespace v6;

struct Context_s
{
	core::u32 gridMacroShift;
	core::u32 gridMacroWidth;
	core::u32 gridMacroHalfWidth;
	float gridScaleMin;
	float gridScaleMax;
	core::u32 frameCount;
	core::u32 mipCount;
};

struct Frame_s
{
	core::CodecFrameDesc_s	desc;
	core::CodecFrameData_s	data;
	core::u32				blockPosCount;
	core::u32				blockDataCount;
	core::u32				blockPosOffsets[CODEC_BUCKET_COUNT];
	core::u32				blockDataOffsets[CODEC_BUCKET_COUNT];
	core::u32*				blockCounters;
	core::Vec3i				gridMin[CODEC_MIP_MAX_COUNT];
	core::Vec3i				gridMax[CODEC_MIP_MAX_COUNT];
	core::u32				finalBlockCounts[CODEC_BUCKET_COUNT][V6_MERGE_SHARED_FRAME_MAX_COUNT];
};

bool Frame_LoadFromFile( Frame_s* frame, const char* filename, core::IAllocator* allocator )
{
	core::CFileReader fileReader;
	if ( !fileReader.Open( filename ) )
	{
		V6_ERROR( "Unable to open %s.\n", filename );
		return false;
	}

	if  ( !core::Codec_ReadFrame( &fileReader, &frame->desc, &frame->data, allocator ) )
	{
		V6_ERROR( "Unable to read %s.\n", filename );
		return false;
	}

	frame->blockPosCount = 0;
	frame->blockDataCount = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		const core::u32 cellCount = frame->desc.blockCounts[bucket] * cellPerBucketCount;
		frame->blockPosOffsets[bucket] = frame->blockPosCount;
		frame->blockDataOffsets[bucket] = frame->blockDataCount;
		frame->blockPosCount += frame->desc.blockCounts[bucket];
		frame->blockDataCount += cellCount;
	}

	frame->blockCounters = allocator->newArray< core::u32 >( frame->blockPosCount );
	memset( frame->blockCounters, 0, frame->blockPosCount * sizeof( core::u32 ) );

	return true;
}

void Frame_Sort( Frame_s* frame, core::u32 frameID, const Context_s* context, core::IStack* allocator )
{
	allocator->push();

	hlsl::blockPositions = (hlsl::uint*)frame->data.blockPos;
	hlsl::blockData = (hlsl::uint*)frame->data.blockData;
	hlsl::bits = allocator->newArray< core::u32 >( HLSL_STREAM_SIZE );
	hlsl::groupBits = allocator->newArray< core::u32 >( HLSL_STREAM_GROUP_SIZE );
	hlsl::counts = allocator->newArray< core::u32 >( HLSL_STREAM_SIZE * 2 );
	hlsl::addresses = allocator->newArray< core::u32 >( HLSL_STREAM_SIZE );

	hlsl::outBlockPositions = allocator->newArray< core::u32 >( frame->blockPosCount );
	hlsl::outBlockData = allocator->newArray< core::u32 >( frame->blockDataCount );

	hlsl::c_mergeOutBlockDataOffset = 0;
	hlsl::c_mergeOutBlockPosOffset = 0;

	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const core::u32 cellPerBucketCount = 1 << (bucket + 2);

		hlsl::c_mergeBlockCount = frame->desc.blockCounts[bucket];
		hlsl::c_mergeBlockPosOffset = frame->blockPosOffsets[bucket];
		hlsl::c_mergeBlockDataOffset = frame->blockDataOffsets[bucket];
		hlsl::c_mergeCellPerBucketCount = cellPerBucketCount;

		for ( core::u32 counter = 0; counter < context->frameCount; ++counter )
		{
			memset( hlsl::bits, 0, HLSL_STREAM_SIZE * sizeof( core::u32 ) );
			memset( hlsl::groupBits, 0, HLSL_STREAM_GROUP_SIZE * sizeof( core::u32 ) );
			memset( hlsl::counts, 0, HLSL_STREAM_SIZE * 2 * sizeof( core::u32 ) );			

			hlsl::c_mergeCounter = counter;
			hlsl::s_Compaction_GetKey = hlsl::Sort_GetKey;
			core::Compute_Dispatch( frame->desc.blockCounts[bucket], HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::Compaction_SetBit< true >, "Compaction_SetBit" );

			core::u32 elementCount = HLSL_STREAM_SIZE;
			hlsl::c_mergeLowerOffset = 0;
			for ( core::u32 layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
			{
				V6_ASSERT( elementCount > 0 );
				hlsl::c_mergeCurrentOffset = hlsl::c_mergeLowerOffset;
				hlsl::c_mergeLowerOffset += elementCount;
				core::Compute_Dispatch( elementCount, HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::Compaction_PrefixSum< HLSL_STREAM_THREAD_GROUP_SIZE >, "Compaction_PrefixSum" );
			}
			V6_ASSERT( elementCount <= 1 );

			core::Compute_Dispatch( HLSL_STREAM_SIZE, HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::Compaction_Summarize< HLSL_STREAM_SIZE, HLSL_STREAM_LAYER_COUNT, HLSL_STREAM_THREAD_GROUP_SIZE >, "Compaction_Summarize" );

			core::u32 sortedBlockCount = hlsl::counts[hlsl::c_mergeLowerOffset];
			frame->finalBlockCounts[bucket][counter] = sortedBlockCount;

			hlsl::s_ScatterBlock = hlsl::Sort_ScatterBlock;
			core::Compute_Dispatch( frame->desc.blockCounts[bucket], HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::Compaction_Scatter, "Compaction_Scatter" );

			hlsl::c_mergeOutBlockPosOffset += sortedBlockCount;
			hlsl::c_mergeOutBlockDataOffset += sortedBlockCount * cellPerBucketCount;
		}

		V6_ASSERT( hlsl::c_mergeOutBlockPosOffset == frame->desc.blockCounts[bucket] );
	}

	V6_ASSERT( hlsl::c_mergeOutBlockPosOffset == frame->blockPosCount );

	memcpy( frame->data.blockPos, hlsl::outBlockPositions, frame->blockPosCount * sizeof( core::u32 ) );
	memcpy( frame->data.blockData, hlsl::outBlockData, frame->blockDataCount * sizeof( core::u32 ) );
	frame->blockCounters = nullptr;

	allocator->pop();
}

void Frame_DumpCounters( const Frame_s* frame, core::u32 frameID )
{
	core::u32 counters[V6_MERGE_SHARED_FRAME_MAX_COUNT] = {};
	for ( core::u32 blockID = 0; blockID < frame->blockPosCount; ++blockID )
	{
		V6_ASSERT( frame->blockCounters[blockID] < V6_MERGE_SHARED_FRAME_MAX_COUNT );
		// V6_MSG( "Frame %d: blockID %d, blockPackedPos 0x%08X, shared %d\n", frameID, blockID, ((int*)(frame->data.blockPos))[blockID], frame->blockCounters[blockID] );
		++counters[frame->blockCounters[blockID]];
	}

	V6_MSG( "Frame %d, repartition of %5u blocks:\n", frameID, frame->blockPosCount );
	for ( core::u32 counter = 0; counter < V6_MERGE_SHARED_FRAME_MAX_COUNT; ++counter )
	{
		if ( counters[counter] )
			V6_MSG( "- %5u blocks are shared %d time(s).\n", counters[counter], counter );
	}

}

void Frame_Compare( Frame_s* frames, core::u32 refFrameID, core::u32 newFrameID, const Context_s* context, core::IStack* allocator )
{
	V6_ASSERT( refFrameID < newFrameID );
	core::u32 mergeMinCounters[2] = { newFrameID - refFrameID - 1, 0 };

	for ( core::u32 mip = 0; mip < context->mipCount; ++mip )
	{
		bool overlap = true; 
		for ( core::u32 axis = 0; axis < 3; ++axis )
		{
			if ( frames[refFrameID].gridMin[mip][axis] >= frames[newFrameID].gridMax[mip][axis] || frames[newFrameID].gridMin[mip][axis] >= frames[refFrameID].gridMax[mip][axis] )
			{
				overlap = false;
				break;
			}
		}

		if ( !overlap )
			continue;

		const core::Vec3i boxMin = core::Min( frames[refFrameID].gridMin[mip], frames[newFrameID].gridMin[mip] );
		const core::Vec3i boxMax = core::Max( frames[refFrameID].gridMax[mip], frames[newFrameID].gridMax[mip] );
		const core::Vec3i boxDim = boxMax - boxMin;

		hlsl::c_mergeMip = mip;

		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			const core::u32 gridMacroDoubleWidth = context->gridMacroWidth << 4;
			V6_ASSERT( boxDim.x <= (int)gridMacroDoubleWidth && boxDim.y <= (int)gridMacroDoubleWidth && boxDim.z <= (int)gridMacroDoubleWidth );
	
			const core::u32 cellPerBucketCount = 1 << (bucket + 2);
			hlsl::c_mergeCellPerBucketCount = cellPerBucketCount;

			allocator->push();

			core::u32* bitBuffers[2] = {};
	
			for ( core::u32 frameRank = 0; frameRank < 2; ++frameRank )
			{
				Frame_s* frame = (frameRank == 0) ? &frames[refFrameID] : &frames[newFrameID];

				hlsl::blockPositions = (hlsl::uint*)frame->data.blockPos;
				hlsl::blockCounters = (hlsl::uint*)frame->blockCounters;

				bitBuffers[frameRank] = allocator->newArray< core::u32 >( HLSL_MERGE_SIZE );
				hlsl::bits = bitBuffers[frameRank];
				memset( hlsl::bits, 0, HLSL_MERGE_SIZE * sizeof( core::u32 ) );

				hlsl::c_mergeBlockPosTranslation = frame->gridMin[mip] - boxMin;
				hlsl::c_mergeBlockCount = frame->desc.blockCounts[bucket];
				hlsl::c_mergeBlockPosOffset = frame->blockPosOffsets[bucket];

				hlsl::c_mergeCounter = mergeMinCounters[frameRank];
				hlsl::s_Compaction_GetKey = hlsl::Compare_GetKey;
				core::Compute_Dispatch( frame->desc.blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compaction_SetBit< false >, "Compare_SetBit" );
			}

			{
				hlsl::refBits = bitBuffers[0];
				hlsl::bits = bitBuffers[1];
				hlsl::groupBits = allocator->newArray< core::u32 >( HLSL_MERGE_GROUP_SIZE );
				memset( hlsl::groupBits, 0, HLSL_MERGE_GROUP_SIZE * sizeof( core::u32 ) );
				core::Compute_Dispatch( HLSL_MERGE_SIZE, HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compare_AndBit, "Compare_AndBit" );
			}
	
			core::u32* blockPositionBuffers[2] = {};
			core::u32* blockDataBuffers[2] = {};

			core::u32 sameBlockCounts[2] = {};

			for ( core::u32 frameRank = 0; frameRank < 2; ++frameRank )
			{
				Frame_s* frame = (frameRank == 0) ? &frames[refFrameID] : &frames[newFrameID];

				hlsl::blockPositions = (hlsl::uint*)frame->data.blockPos;
				hlsl::blockData = (hlsl::uint*)frame->data.blockData;
				hlsl::counts = allocator->newArray< core::u32 >( HLSL_MERGE_SIZE * 2 );
				hlsl::addresses = allocator->newArray< core::u32 >( HLSL_MERGE_SIZE );

				memset( hlsl::counts, 0, HLSL_MERGE_SIZE * 2 * sizeof( core::u32 ) );

				core::u32 countBuffer[HLSL_MERGE_THREAD_GROUP_SIZE];
				hlsl::g_counts = countBuffer;
				hlsl::c_mergeLowerOffset = 0;
				core::u32 elementCount = HLSL_MERGE_SIZE;
				for ( core::u32 layer = 0; layer < HLSL_MERGE_LAYER_COUNT; ++layer, elementCount >>= HLSL_MERGE_THREAD_GROUP_SHIFT )
				{
					V6_ASSERT( elementCount > 0 );
					hlsl::c_mergeCurrentOffset = hlsl::c_mergeLowerOffset;
					hlsl::c_mergeLowerOffset += elementCount;
					core::Compute_Dispatch( elementCount, HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compaction_PrefixSum< HLSL_MERGE_THREAD_GROUP_SIZE >, "Compaction_PrefixSum" );
				}
				V6_ASSERT( elementCount <= 1 );

				core::Compute_Dispatch( HLSL_MERGE_SIZE, HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compaction_Summarize< HLSL_MERGE_SIZE, HLSL_MERGE_LAYER_COUNT, HLSL_MERGE_THREAD_GROUP_SIZE >, "Compaction_Summarize" );

				sameBlockCounts[frameRank] = hlsl::counts[hlsl::c_mergeLowerOffset];
		
				{
					hlsl::c_mergeBlockCount = frame->desc.blockCounts[bucket];
					hlsl::c_mergeBlockPosOffset = frame->blockPosOffsets[bucket];
					hlsl::c_mergeBlockDataOffset = frame->blockDataOffsets[bucket];

					blockPositionBuffers[frameRank] = allocator->newArray< core::u32 >( sameBlockCounts[frameRank] );
					blockDataBuffers[frameRank] = allocator->newArray< core::u32 >( sameBlockCounts[frameRank] * cellPerBucketCount );
					hlsl::outBlockPositions = blockPositionBuffers[frameRank];
					hlsl::outBlockData = blockDataBuffers[frameRank];
					
					hlsl::c_mergeCounter = mergeMinCounters[frameRank];
					hlsl::s_ScatterBlock = hlsl::Compare_ScatterBlock;
					core::Compute_Dispatch( frame->desc.blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compaction_Scatter, "Compaction_Scatter" );
				}
			}

			V6_ASSERT( sameBlockCounts[0] == sameBlockCounts[1] );
#if 0
			if ( sameBlockCounts[1] != 0 )
				V6_MSG( "Sharing %5u blocks at mip %d, bucket %d\n", sameBlockCounts[1], mip, bucket );
#endif

			hlsl::bits = bitBuffers[1];

			{
				hlsl::refBlockPositions = blockPositionBuffers[0];
				hlsl::blockPositions = blockPositionBuffers[1];
				hlsl::refBlockData = blockDataBuffers[0];
				hlsl::blockData = blockDataBuffers[1];

				hlsl::c_mergeRefBlockPosTranslation = frames[refFrameID].gridMin[mip] - boxMin;
				hlsl::c_mergeBlockPosTranslation = frames[newFrameID].gridMin[mip] - boxMin;

				core::Compute_Dispatch( sameBlockCounts[1], HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compare_UnsetBit, "Compare_UnsetBit" );
			}

			for ( core::u32 frameRank = 0; frameRank < 2; ++frameRank )
			{
				Frame_s* frame = (frameRank == 0) ? &frames[refFrameID] : &frames[newFrameID];

				hlsl::blockPositions = (hlsl::uint*)frame->data.blockPos;
				hlsl::blockCounters = frame->blockCounters;

				hlsl::c_mergeBlockPosTranslation = frame->gridMin[mip] - boxMin;

				hlsl::c_mergeBlockCount = frame->desc.blockCounts[bucket];
				hlsl::c_mergeBlockPosOffset = frame->blockPosOffsets[bucket];

				core::Compute_Dispatch( frame->desc.blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Compare_IncCounter, "Compare_IncCounter" );
			}

			allocator->pop();
		}
	}
}

void Frame_TrimShared( Frame_s* frame, core::IStack* allocator )
{
	allocator->push();

	hlsl::MergeContext mergeContext = {};

	hlsl::blockCounters = frame->blockCounters;
	hlsl::mergeContext = &mergeContext;
	hlsl::blockPositions = (hlsl::uint*)frame->data.blockPos;
	hlsl::blockData = (hlsl::uint*)frame->data.blockData;
	hlsl::outBlockPositions = allocator->newArray< core::u32 >( frame->blockPosCount );
	hlsl::outBlockData = allocator->newArray< core::u32 >( frame->blockDataCount );

	core::u32 prevBlockCount = 0;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
	{
		const core::u32 cellPerBucketCount = 1 << (bucket + 2);

		hlsl::c_mergeBlockCount = frame->desc.blockCounts[bucket];
		hlsl::c_mergeBlockPosOffset = frame->blockPosOffsets[bucket];
		hlsl::c_mergeBlockDataOffset  = frame->blockDataOffsets[bucket];
		hlsl::c_mergeCellPerBucketCount = cellPerBucketCount;

		core::Compute_Dispatch( frame->desc.blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::TrimShared, "TrimShared" );

		frame->desc.blockCounts[bucket] = mergeContext.blockPosOffset - prevBlockCount;
		prevBlockCount = mergeContext.blockPosOffset;
	}

	memcpy( frame->data.blockPos, hlsl::outBlockPositions, mergeContext.blockPosOffset * sizeof( core::u32 ) );
	memcpy( frame->data.blockData, hlsl::outBlockData, mergeContext.blockDataOffset * sizeof( core::u32 ) );
	memset( frame->blockCounters, 0, mergeContext.blockPosOffset * sizeof( core::u32 ) );

	frame->blockPosCount = 0;
	frame->blockDataCount = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		const core::u32 cellCount = frame->desc.blockCounts[bucket] * cellPerBucketCount;
		frame->blockPosOffsets[bucket] = frame->blockPosCount;
		frame->blockDataOffsets[bucket] = frame->blockDataCount;
		frame->blockPosCount += frame->desc.blockCounts[bucket];
		frame->blockDataCount += cellCount;
	}

	allocator->pop();
}

int main()
{
	V6_MSG( "Merge 0.0\n" );

	core::CHeap heap;
	core::Stack stack( &heap, 100 * 1024 * 1024 );

	Context_s context = {};

	context.frameCount = 2;
	const char* filenameTemplate = "D:/media/obj/crytek-sponza/sponza_%06d.v6f";

	if ( context.frameCount < 1 )
		return 0;

	core::Compute_Init();

	Frame_s* frames = stack.newArray< Frame_s >( context.frameCount );
	memset( frames, 0, context.frameCount * sizeof( Frame_s ) );

	for ( core::u32 frameID = 0; frameID < context.frameCount; ++frameID )
	{
		Frame_s* frame = &frames[frameID];

		char filename[256];
		sprintf_s( filename, sizeof( filename ), filenameTemplate, frameID /*== 0 ? 0 : 2*/ );

		if ( !Frame_LoadFromFile( frame, filename, &stack ) )
			return 1;

		if ( frameID == 0 )
		{
			context.gridMacroShift = frames[0].desc.gridMacroShift;
			context.gridMacroWidth = 1 << context.gridMacroShift;
			context.gridScaleMin = frames[0].desc.gridScaleMin;
			context.gridScaleMax = frames[0].desc.gridScaleMax;
			context.gridMacroHalfWidth = context.gridMacroWidth >> 1;
			context.mipCount = core::Codec_GetMipCount( context.gridScaleMin, context.gridScaleMax );
		}
		else
		{
			if ( frame->desc.gridMacroShift != context.gridMacroShift )
			{
				V6_ERROR( "Incompatible grid resolution.\n" );
				return 1;
			}

			if ( frame->desc.gridScaleMin != context.gridScaleMin || frame->desc.gridScaleMax != context.gridScaleMax )
			{
				V6_ERROR( "Incompatible grid scales.\n" );
				return 1;
			}
		}

		float gridScale = context.gridScaleMin;
		for ( core::u32 mip = 0; mip < context.mipCount; ++mip, gridScale *= 2.0f )
		{
			const float invBlockSize = context.gridMacroHalfWidth / gridScale;
			const core::Vec3 gridOrg = frame->desc.origin * invBlockSize;
			const core::Vec3i gridCoord = core::Vec3i_Make( (int)floorf( gridOrg.x ), (int)floorf( gridOrg.y ), (int)floorf( gridOrg.z ) );
			frame->gridMin[mip] = gridCoord - (int)context.gridMacroHalfWidth;
			frame->gridMax[mip] = gridCoord + (int)context.gridMacroHalfWidth;
		}

		V6_MSG( "Frame %d: loaded %d blocks from %s.\n", frameID, frame->blockPosCount, filename );
	}

	for ( core::u32 newFrameID = 1; newFrameID < context.frameCount; ++newFrameID )
	{
		Frame_s* newFrame = &frames[newFrameID];
		
		for ( core::u32 refFrameID = 0; refFrameID < newFrameID; ++refFrameID )
		{
			Frame_Compare( frames, refFrameID, newFrameID, &context, &stack );

			const core::u32 prevBlockCount = newFrame->blockPosCount;
			Frame_TrimShared( newFrame, &stack );

			V6_MSG( "Frame %02d.%02d: %d/%d blocks trimmed.\n", newFrameID, refFrameID, prevBlockCount-newFrame->blockPosCount, prevBlockCount );
		}
	}

#if 1
	for ( core::u32 frameID = 0; frameID < context.frameCount; ++frameID )
		Frame_DumpCounters( &frames[frameID], frameID );
#endif

#if 0
	for ( core::u32 frameID = 0; frameID < context.frameCount; ++frameID )
		Frame_Sort( &frames[frameID], frameID, &context, &stack );
#endif

	core::Compute_Release();

	return 0;
}
