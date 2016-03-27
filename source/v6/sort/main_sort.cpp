/*V6*/

#include <v6/core/common.h>
#include <v6/core/compute.h>
#include <v6/core/memory.h>

BEGIN_V6_HLSL_NAMESPACE

#define GRID_CELL_BUCKET				0

#define HLSL_GRID_MACRO_SHIFT			5
#define HLSL_GRID_MACRO_2XSHIFT			(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT			(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_WIDTH			(1 << HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_MASK			(HLSL_GRID_MACRO_WIDTH-1)

#define HLSL_STREAM_THREAD_GROUP_SHIFT	4
#define HLSL_STREAM_THREAD_GROUP_SIZE	(1 << HLSL_STREAM_THREAD_GROUP_SHIFT)

#define HLSL_STREAM_SHIFT				((HLSL_GRID_MACRO_3XSHIFT + 4) - 5)
#define HLSL_STREAM_SIZE				(1 << HLSL_STREAM_SHIFT)
#define HLSL_STREAM_GROUP_SIZE			((HLSL_STREAM_SIZE >> HLSL_STREAM_THREAD_GROUP_SHIFT)>>5)
#define HLSL_STREAM_LAYER_COUNT			((HLSL_STREAM_SHIFT + HLSL_STREAM_THREAD_GROUP_SHIFT - 1) / HLSL_STREAM_THREAD_GROUP_SHIFT)

#define block_count( BUCKET )			1000
#define block_posOffset( BUCKET )		0

uint blockPositions[block_count( 0 )] = {};
uint sortedBlockPositions[block_count( 0 )] = {};
uint streamGroupBits[HLSL_STREAM_GROUP_SIZE] = {};
uint streamBits[HLSL_STREAM_SIZE] = {};
uint streamCounts[HLSL_STREAM_SIZE * 2] = {};
uint streamAddresses[HLSL_STREAM_SIZE] = {};

uint c_streamCurrentOffset = 0;
uint c_streamLowerOffset = 0;

void ScatterBlock( uint packedBlockPos, uint prevBlockID, uint newBlockID )
{
	sortedBlockPositions[newBlockID] = packedBlockPos;
}

#define EXPORT_STREAM_SET_BIT		1
#define EXPORT_STREAM_PREFIX_SUM	1
#define EXPORT_STREAM_SUMMARIZE		1
#define EXPORT_STREAM_SCATTER		1
#include <v6/viewer/stream_scan_cs_impl.hlsli>

END_V6_HLSL_NAMESPACE

using namespace v6;

static int CompareBlockPositionByKey( const void* p0, const void* p1 )
{
	const __int64 s0 = hlsl::ComputeKeyFromPackedBlockPos( *((core::u32*)p0) );
	const __int64 s1 = hlsl::ComputeKeyFromPackedBlockPos( *((core::u32*)p1) );
	if ( s0 == s1 )
		return 0;
	return s0 < s1 ? -1 : 1;
}

bool TestAndSetBit( core::u32 packedBlockPos )
{
	const core::u32 key = hlsl::ComputeKeyFromPackedBlockPos( packedBlockPos );

	const core::u32 chunk = key >> 5;
	const core::u32 bit = key & 0x1F;
	const core::u32 bitMask = 1 << bit;

	if ( hlsl::streamBits[chunk] & bitMask )
		return true;

	hlsl::streamBits[chunk] |= bitMask;
	return false;
}

int main()
{
	V6_MSG( "Sort 0.0\n" );

	core::CHeap heap;
	core::Stack stack( &heap, 100 * 1024 * 1024 );

	V6_MSG( "Blocks:");
	for ( core::u32 blockID = 0; blockID < (core::u32)block_count( GRID_CELL_BUCKET ); )
	{
		const core::u32 mip = rand() & 0xF;
		const core::u32 x = rand() & HLSL_GRID_MACRO_MASK;
		const core::u32 y = rand() & HLSL_GRID_MACRO_MASK;
		const core::u32 z = rand() & HLSL_GRID_MACRO_MASK;
		const core::u32 blockPos = (z << HLSL_GRID_MACRO_2XSHIFT) | (y << HLSL_GRID_MACRO_SHIFT) | x;
		const core::u32 packedBlockPos = (mip << 28) | blockPos;
		if ( TestAndSetBit( packedBlockPos ) )
			continue;

		hlsl::blockPositions[blockID] = packedBlockPos;
		if ( blockID < 16 )
			V6_PRINT( " 0x%08X", packedBlockPos );

		++blockID;
	}
	V6_PRINT( "\n" );

	core::Compute_Init();

	core::Compute_Dispatch( block_count( GRID_CELL_BUCKET ), HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::SetBit, "SetBit" );

	core::u32 elementCount = HLSL_STREAM_SIZE;
	hlsl::c_streamLowerOffset = 0;
	for ( core::u32 layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
	{
		V6_ASSERT( elementCount > 0 );
		hlsl::c_streamCurrentOffset = hlsl::c_streamLowerOffset;
		hlsl::c_streamLowerOffset += elementCount;
		core::Compute_Dispatch( elementCount, HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::PrefixSum, "PrefixSum" );
	}
	V6_ASSERT( elementCount <= 1 );

	core::Compute_Dispatch( HLSL_STREAM_SIZE, HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::Summarize, "Summarize" );

	core::Compute_Dispatch( block_count( GRID_CELL_BUCKET ), HLSL_STREAM_THREAD_GROUP_SIZE, hlsl::Scatter, "Scatter" );

	core::Compute_Release();

	qsort( hlsl::blockPositions, block_count( GRID_CELL_BUCKET ), 4, CompareBlockPositionByKey );

	for ( core::u32 blockID = 0; blockID < (core::u32)block_count( GRID_CELL_BUCKET ); ++blockID )
		V6_ASSERT( hlsl::blockPositions[blockID] == hlsl::sortedBlockPositions[blockID] );

	return 0;
}