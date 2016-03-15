/*V6*/

#pragma comment( lib, "core.lib" )

#include <v6/core/common.h>

#include <v6/core/codec.h>
#include <v6/core/compute.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/vec3i.h>

#include <v6/viewer/common_shared.h>

#define MERGE_FRAME_MAX_COUNT	600

BEGIN_V6_HLSL_NAMESPACE

uint* blockPositions = nullptr;
uint* blockData = nullptr;
uint* blockIndirectArgs = nullptr;
uint* bits = nullptr;
uint* groupBits = nullptr;
uint* counts = nullptr;
uint* addresses = nullptr;
uint* sortedBlockIDs = nullptr;
uint* sortedBlockPositions = nullptr;
uint* sortedBlockData = nullptr;

uint c_mergeCurrentOffset;
uint c_mergeLowerOffset;

uint c_mergeMip;
int3 c_mergeBlockPosOffset;

#define INTERLEAVE_S( S, SHIFT, OFFSET )				(((S >> SHIFT) & 1) << (SHIFT * 3 + OFFSET))
#define INTERLEAVE_X( X, SHIFT )						INTERLEAVE_S( X, SHIFT, 0 )
#define INTERLEAVE_Y( Y, SHIFT )						INTERLEAVE_S( Y, SHIFT, 1 )
#define INTERLEAVE_Z( Z, SHIFT )						INTERLEAVE_S( Z, SHIFT, 2 )

uint ComputeKeyFromBlockPos( uint blockPos, int3 blockPosOffset )
{
	const uint x = ((blockPos >> 0)							& HLSL_GRID_MACRO_MASK) + blockPosOffset.x;
	const uint y = ((blockPos >> HLSL_GRID_MACRO_SHIFT)		& HLSL_GRID_MACRO_MASK) + blockPosOffset.y;
	const uint z = ((blockPos >> HLSL_GRID_MACRO_2XSHIFT)	& HLSL_GRID_MACRO_MASK) + blockPosOffset.z;

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

DEFINE( GRID_CELL_BUCKET )
void SetBitForSort( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < block_count( GRID_CELL_BUCKET ) )
	{
		const uint posOffset = block_posOffset( GRID_CELL_BUCKET );
		const uint blockPosID = posOffset + blockID;

		const uint key = ComputeKeyFromPackedBlockPos( blockPositions[blockPosID] );

		const uint chunk = key >> 5;
		const uint bit = key & 0x1F;

		InterlockedOr( bits[chunk], 1 << bit );

		const uint keyGroup = chunk >> HLSL_STREAM_THREAD_GROUP_SHIFT;

		const uint chunkGroup = keyGroup >> 5;
		const uint bitGroup = keyGroup & 0x1F;

		InterlockedOr( groupBits[chunkGroup], 1 << bitGroup );
	}
}

groupshared uint g_counts[HLSL_STREAM_THREAD_GROUP_SIZE];

void PrefixSum( uint groupID, uint threadGroupID, uint threadID )
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
			if ( c_mergeLowerOffset != 0 )
				counts[c_mergeLowerOffset + groupID] = g_counts[HLSL_STREAM_THREAD_GROUP_SIZE-1];
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

	counts[c_mergeCurrentOffset + threadID] = g_counts[threadGroupID];
}

void Summarize( uint groupID, uint threadGroupID, uint threadID )
{
	const uint chunkGroup = groupID >> 5;
	const uint bitGroup = groupID & 0x1F;

	if ( (groupBits[chunkGroup] & (1 << bitGroup)) == 0 )
		return;

	uint address = 0;

	uint elementCount = HLSL_STREAM_SIZE;
	uint offsetID = threadID;
	uint streamOffset = 0;
	for ( uint layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT, offsetID >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
	{
		address += counts[streamOffset + offsetID];
		streamOffset += elementCount;
	}

	addresses[threadID] = address;
}

DEFINE( GRID_CELL_BUCKET )
void ScatterBlock( uint packedBlockPos, uint prevBlockID, uint newBlockID )
{
	const uint GRID_CELL_SHIFT = GRID_CELL_BUCKET + 2;
	const uint GRID_CELL_COUNT = 1 << GRID_CELL_SHIFT;

	const uint posOffset = block_posOffset( GRID_CELL_BUCKET );
	const uint blockPosID = posOffset + newBlockID;
	sortedBlockIDs[blockPosID] = blockPosID;
	sortedBlockPositions[blockPosID] = packedBlockPos;

	const uint dataOffset = block_dataOffset( GRID_CELL_BUCKET );
	const uint dataSize = GRID_CELL_COUNT;

	const uint srcDataBaseID = dataOffset + prevBlockID * dataSize;
	const uint dstDataBaseID = dataOffset + newBlockID * dataSize;

	for ( uint cellID = 0; cellID < GRID_CELL_COUNT; ++cellID )
		sortedBlockData[dstDataBaseID + cellID] = blockData[srcDataBaseID + cellID];
}

DEFINE( GRID_CELL_BUCKET )
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

		const uint prevBits = bits[chunk] & prevBitMask;
		const uint rank = countbits( prevBits );

		const uint finalAddress = addresses[chunk] + rank;

		ScatterBlock< GRID_CELL_BUCKET >( packedBlockPos, blockID, finalAddress );
	}
}

#if 0

DEFINE( GRID_CELL_BUCKET )
void SetBit( uint groupID, uint threadGroupID, uint threadID )
{
	const uint blockID = threadID;
	if ( blockID < block_count( GRID_CELL_BUCKET ) )
	{
		const uint posOffset = block_posOffset( GRID_CELL_BUCKET );
		const uint blockPosID = posOffset + blockID;

		const uint packedBlockPos = blockPositions[blockPosID];
		const uint mip = packedBlockPos >> 28;

		if ( mip == c_mergeMip )
		{
			const uint blockPos = packedBlockPos & 0x0FFFFFFF;
			const uint key = ComputeKeyFromBlockPos( blockPos, c_mergeBlockPosOffset );

			const uint chunk = key >> 5;
			const uint bit = key & 0x1F;

			InterlockedOr( mergeBitsA[chunk], 1 << bit );

			const uint keyGroup = chunk >> HLSL_MERGE_THREAD_GROUP_SHIFT;

			const uint chunkGroup = keyGroup >> 5;
			const uint bitGroup = keyGroup & 0x1F;

			InterlockedOr( mergeGroupBits[chunkGroup], 1 << bitGroup );
		}
	}
}

void CompareBit( uint groupID, uint threadGroupID, uint threadID )
{
	mergeCommonBits[threadID] = mergeBitsA[threadID] & mergeBitsB[threadID];
}

#endif

END_V6_HLSL_NAMESPACE

using namespace v6;

int main()
{
	V6_MSG( "Merge 0.0\n" );

	core::CHeap heap;
	core::Stack stack( &heap, 100 * 1024 * 1024 );

	const core::u32 fileCount = 2;
	const char* filenames[fileCount] = 
	{
		"D:/media/obj/crytek-sponza/sponza_000000.v6f",
		"D:/media/obj/crytek-sponza/sponza_000001.v6f"
	};

	if ( fileCount < 1 )
		return 0;

	core::Compute_Init();

	{
		stack.push();

		core::CodecFrameDesc_s frameDescRef;
		core::CodecFrameData_s frameDataRef;

		{
			core::CFileReader fileReader;
			if ( !fileReader.Open( filenames[0] ) )
			{
				V6_ERROR( "Unable to open %s.\n", filenames[0] );
				return 1;
			}

			if  ( !core::Codec_ReadFrame( &fileReader, &frameDescRef, &frameDataRef, &stack ) )
			{
				V6_ERROR( "Unable to read %s.\n", filenames[0] );
				return 1;
			}
		}

		core::u32 frame = 0;

		core::u32 blockIndirectArgBuffer[block_all_offset] = {};
		core::u32 blockPosCount = 0;
		core::u32 blockDataCount = 0;

		core::u32 cellPerBucketCount = 4;
		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
		{
			const core::u32 cellCount = frameDescRef.blockCounts[bucket] * cellPerBucketCount;

			blockIndirectArgBuffer[block_count_offset( bucket )] = frameDescRef.blockCounts[bucket];
			blockIndirectArgBuffer[block_posOffset_offset( bucket )] = blockPosCount;
			blockIndirectArgBuffer[block_dataOffset_offset( bucket )] = blockDataCount;
			blockIndirectArgBuffer[block_cellCount_offset( bucket )] = cellCount;

			blockPosCount += frameDescRef.blockCounts[bucket];
			blockDataCount += cellCount;
		}

		V6_MSG( "%d blocks loaded from frame %d\n", blockPosCount, frame );

		core::u32* allBlockIDs[MERGE_FRAME_MAX_COUNT] = {};
		core::u32* allBlockPositions[MERGE_FRAME_MAX_COUNT] = {};
		core::u32* allBlockData[MERGE_FRAME_MAX_COUNT] = {};
		
		allBlockIDs[frame] = stack.newArray< core::u32 >( blockPosCount );
		allBlockPositions[frame] = stack.newArray< core::u32 >( blockPosCount );
		allBlockData[frame] = stack.newArray< core::u32 >( blockDataCount );
		
		stack.push();
		
		hlsl::blockPositions = (hlsl::uint*)frameDataRef.blockPos;
		hlsl::blockData = (hlsl::uint*)frameDataRef.blockData;
		hlsl::blockIndirectArgs = blockIndirectArgBuffer;
		hlsl::bits = stack.newArray< core::u32 >( HLSL_STREAM_SIZE );
		hlsl::groupBits = stack.newArray< core::u32 >( HLSL_STREAM_GROUP_SIZE );
		hlsl::counts = stack.newArray< core::u32 >( HLSL_STREAM_SIZE * 2 );
		hlsl::addresses = stack.newArray< core::u32 >( HLSL_STREAM_SIZE );

		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			memset( hlsl::bits, 0, HLSL_STREAM_SIZE * sizeof( core::u32 ) );
			memset( hlsl::groupBits, 0, HLSL_STREAM_GROUP_SIZE * sizeof( core::u32 ) );
			memset( hlsl::counts, 0, HLSL_STREAM_SIZE * 2 * sizeof( core::u32 ) );

			static const core::Compute_DispatchKernel_f setBitKernels[CODEC_BUCKET_COUNT] = { hlsl::SetBitForSort<0>, hlsl::SetBitForSort<1>, hlsl::SetBitForSort<2>, hlsl::SetBitForSort<3>, hlsl::SetBitForSort<4> };
			core::Compute_Dispatch( frameDescRef.blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, setBitKernels[bucket], "SetBitForSort" );

			core::u32 elementCount = HLSL_STREAM_SIZE;
			hlsl::c_mergeLowerOffset = 0;
			for ( core::u32 layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
			{
				V6_ASSERT( elementCount > 0 );
				hlsl::c_mergeCurrentOffset = hlsl::c_mergeLowerOffset;
				hlsl::c_mergeLowerOffset += elementCount;
				core::Compute_Dispatch( elementCount, HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::PrefixSum, "PrefixSum" );
			}
			V6_ASSERT( elementCount <= 1 );

			core::Compute_Dispatch( HLSL_STREAM_SIZE, HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::Summarize, "Summarize" );
			
			hlsl::sortedBlockIDs = allBlockIDs[frame];
			hlsl::sortedBlockPositions = allBlockPositions[frame];
			hlsl::sortedBlockData = allBlockData[frame];
			static const core::Compute_DispatchKernel_f scatterKernels[CODEC_BUCKET_COUNT] = { hlsl::Scatter<0>, hlsl::Scatter<1>, hlsl::Scatter<2>, hlsl::Scatter<3>, hlsl::Scatter<4> };
			core::Compute_Dispatch( frameDescRef.blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, scatterKernels[bucket], "Scatter" );
		}

		core::u32 offset = 0;
		for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			int prevBlockPos = -1;
			for ( core::u32 blockRank = 0; blockRank < frameDescRef.blockCounts[bucket]; ++blockRank )
			{
				const core::u32 blockID = blockRank + offset;
				printf( "Block %d: %d, %d\n", blockID, allBlockIDs[frame][blockID], allBlockPositions[frame][blockID] );
				V6_ASSERT( blockID == allBlockIDs[frame][blockID] );
				V6_ASSERT( prevBlockPos < (int)allBlockPositions[frame][blockID] );
				prevBlockPos = (int)allBlockPositions[blockID];
			}
			offset += frameDescRef.blockCounts[bucket];
		}

		stack.pop();

#if 0
		core::CodecFrameDesc_s frameDesc[2];
		core::CodecFrameData_s frameData[2];

		for ( core::u32 frame = 0; frame < 2; ++frame )
		{
			core::CFileReader fileReader;
			if ( !fileReader.Open( filenames[frame] ) )
			{
				V6_ERROR( "Unable to open %s.\n", filenames[frame] );
				return 1;
			}

			if  ( !core::Codec_ReadFrame( &fileReader, &frameDesc[frame], &frameData[frame], &stack ) )
			{
				V6_ERROR( "Unable to read %s.\n", filenames[frame] );
				return 1;
			}
		}

		if ( frameDesc[0].gridMacroShift != frameDesc[1].gridMacroShift )
		{
			V6_ERROR( "Incompatible grid resolution.\n" );
			return 1;
		}

		if ( frameDesc[0].gridScaleMin != frameDesc[1].gridScaleMin || frameDesc[0].gridScaleMax != frameDesc[1].gridScaleMax )
		{
			V6_ERROR( "Incompatible grid scales.\n" );
			return 1;
		}

		core::u32 blockIndirectArgBuffers[2][block_all_offset] = {};

		for ( core::u32 frame = 0; frame < 2; ++frame )
		{
			core::u32 blockPosCount = 0;
			core::u32 blockDataCount = 0;

			core::u32 cellPerBucketCount = 4;
			for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
			{
				const core::u32 cellCount = frameDesc[frame].blockCounts[bucket] * cellPerBucketCount;

				blockIndirectArgBuffers[frame][block_count_offset( bucket )] = frameDesc[frame].blockCounts[bucket];
				blockIndirectArgBuffers[frame][block_posOffset_offset( bucket )] = blockPosCount;
				blockIndirectArgBuffers[frame][block_dataOffset_offset( bucket )] = blockDataCount;
				blockIndirectArgBuffers[frame][block_cellCount_offset( bucket )] = cellCount;

				blockPosCount += frameDesc[frame].blockCounts[bucket];
				blockDataCount += cellCount;
			}

			V6_MSG( "%d blocks in frame %d\n", blockPosCount, frame );
		}

		const core::u32 gridMacroWidth = 1 << frameDesc[0].gridMacroShift;
		const core::u32 gridMacroHalfWidth = gridMacroWidth >> 1;
		const core::u32 mipCount = core::Codec_GetMipCount( &frameDesc[0] );

		float gridScale = frameDesc[0].gridScaleMin;
		for ( core::u32 mip = 0; mip < mipCount; ++mip, gridScale *= 2.0f )
		{
			V6_MSG( "\n" );
			V6_MSG( "Mip %d\n", mip );

			const float invBlockSize = gridMacroHalfWidth / gridScale;
			core::Vec3i gridMin[2];
			core::Vec3i gridMax[2];
			for ( core::u32 frame = 0; frame < 2; ++frame )
			{
				const core::Vec3 gridOrg = frameDesc[frame].origin * invBlockSize;
				const core::Vec3i gridCoord = core::Vec3i_Make( (int)floorf( gridOrg.x ), (int)floorf( gridOrg.y ), (int)floorf( gridOrg.z ) );
				gridMin[frame] = gridCoord - (int)gridMacroHalfWidth;
				gridMax[frame] = gridCoord + (int)gridMacroHalfWidth;

				V6_MSG( "Grid box %d: (%d %d %d) (%d %d %d)\n", frame,
					gridMin[frame].x, gridMin[frame].y, gridMin[frame].z,
					gridMax[frame].x, gridMax[frame].y, gridMax[frame].z );
			}

			bool overlap = true; 
			for ( core::u32 axis = 0; axis < 3; ++axis )
			{
				if ( gridMin[0][axis] > gridMax[1][axis] || gridMin[1][axis] > gridMax[0][axis] )
				{
					overlap = false;
					break;
				}
			}

			if ( !overlap )
			{
				V6_MSG( "Not overlapping grids.\n" );
				return 1;
			}

			const core::Vec3i boxMin = core::Min( gridMin[0], gridMin[1] );
			const core::Vec3i boxMax = core::Max( gridMax[0], gridMax[1] );
			const core::Vec3i boxDim = boxMax - boxMin;
			
			const core::u32 gridMacroDoubleWidth = gridMacroWidth << 4;
			V6_ASSERT( boxDim.x <= (int)gridMacroDoubleWidth && boxDim.y <= (int)gridMacroDoubleWidth && boxDim.z <= (int)gridMacroDoubleWidth );

			V6_MSG( "Grid delta: (%d %d %d)\n", boxDim.x - gridMacroWidth, boxDim.y - gridMacroWidth, boxDim.z - gridMacroWidth );

			core::u32 mergeBitBuffers[3][HLSL_MERGE_SIZE] = {};
			memset( hlsl::mergeGroupBits, 0, sizeof( hlsl::mergeGroupBits ) );

			for ( core::u32 frame = 0; frame < 2; ++frame )
			{
				hlsl::blockPositions = (hlsl::uint*)frameData[frame].blockPos;
				hlsl::blockIndirectArgs = blockIndirectArgBuffers[frame];
				hlsl::mergeBitsA = mergeBitBuffers[frame];
				memset( hlsl::mergeBitsA, 0, sizeof( mergeBitBuffers[frame] ) );

				hlsl::c_mergeMip = mip;
				hlsl::c_mergeBlockPosOffset = gridMin[frame] - boxMin;

				for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
				{
					static const core::Compute_DispatchKernel_f kernels[CODEC_BUCKET_COUNT] = { hlsl::SetBit<0>, hlsl::SetBit<1>, hlsl::SetBit<2>, hlsl::SetBit<3>, hlsl::SetBit<4> };
					char str[64];
					sprintf_s( str, sizeof( str ), "SetBit_Frame%d_Bucket%d", frame, bucket ) ;
					core::Compute_Dispatch( frameDesc[frame].blockCounts[bucket], HLSL_MERGE_THREAD_GROUP_SIZE, kernels[bucket], str );
				}
			}

			{
				hlsl::mergeBitsA = mergeBitBuffers[0];
				hlsl::mergeBitsB = mergeBitBuffers[1];
				hlsl::mergeCommonBits = mergeBitBuffers[2];
				memset( hlsl::mergeCommonBits, 0, sizeof( mergeBitBuffers[2] ) );
				core::Compute_Dispatch( HLSL_MERGE_SIZE, HLSL_MERGE_THREAD_GROUP_SIZE, hlsl::CompareBit, "CompareBit" );
			}
		}
#endif

		stack.pop();
	}

	core::Compute_Release();

	return 0;
}
