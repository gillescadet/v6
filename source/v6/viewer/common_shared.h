/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define CONCAT( X, Y )								X ## Y
#define GROUP_COUNT( C, S )							(((C) + (S) - 1)) / (S)

#define HLSL_ENCODE_DATA							0
#define HLSL_STEREO									0

#define HLSL_DEBUG_OCCUPANCY						0
#define HLSL_DEBUG_COLLECT							1
#define HLSL_DEBUG_PIXEL							0

#define HLSL_TRILINEAR_SLOT							0

#define HLSL_SURFACE_SLOT							0
#define HLSL_LCOLOR_SLOT							1
#define HLSL_RCOLOR_SLOT							2
#define HLSL_DEPTH_SLOT								3

#define HLSL_SAMPLE_SLOT							4
#define HLSL_SAMPLE_INDIRECT_ARGS_SLOT				5
#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT			6
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT			7
#define HLSL_OCTREE_LEAF_SLOT						8
#define HLSL_OCTREE_INDIRECT_ARGS_SLOT				9
#define HLSL_BLOCK_POS_SLOT							10
#define HLSL_BLOCK_DATA_SLOT						11
#define HLSL_BLOCK_INDIRECT_ARGS_SLOT				12
#define HLSL_BLOCK_CELL_ITEM_SLOT					13
#define HLSL_BLOCK_CELL_ITEM_COUNT_SLOT				14
#define HLSL_BLOCK_DEBUG_SLOT						15
#define HLSL_CULL_STATS_SLOT						16
#define HLSL_CULL_DEBUG_SLOT						17
#define HLSL_TRACE_CELLS_SLOT						18
#define HLSL_TRACE_INDIRECT_ARGS_SLOT				22
#define HLSL_TRACE_STATS_SLOT						23
#define HLSL_TRACE_DEBUG_SLOT						24
#define HLSL_PIXEL_DEBUG_SLOT						26

#define HLSL_GENERIC_ALBEDO_SLOT					4
#define HLSL_GENERIC_ALPHA_SLOT						5

#define HLSL_TRILINEAR_SAMPLER						CONCAT( s, HLSL_TRILINEAR_SLOT )

#define HLSL_SURFACE_SRV							CONCAT( t, HLSL_SURFACE_SLOT )
#define HLSL_LCOLOR_SRV								CONCAT( t, HLSL_LCOLOR_SLOT )
#define HLSL_RCOLOR_SRV								CONCAT( t, HLSL_RCOLOR_SLOT )
#define HLSL_DEPTH_SRV								CONCAT( t, HLSL_DEPTH_SLOT )

#define HLSL_SAMPLE_SRV								CONCAT( t, HLSL_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_SRV						CONCAT( t, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_BLOCK_POS_SRV							CONCAT( t, HLSL_BLOCK_POS_SLOT )
#define HLSL_BLOCK_DATA_SRV							CONCAT( t, HLSL_BLOCK_DATA_SLOT )
#define HLSL_BLOCK_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_BLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_BLOCK_CELL_ITEM_SRV					CONCAT( t, HLSL_BLOCK_CELL_ITEM_SLOT )
#define HLSL_BLOCK_FIRST_CELL_ITEM_ID_SRV			CONCAT( t, HLSL_BLOCK_FIRST_CELL_ITEM_ID_SLOT )
#define HLSL_BLOCK_CELL_ITEM_COUNT_SRV				CONCAT( t, HLSL_BLOCK_CELL_ITEM_COUNT_SLOT )
#define HLSL_BLOCK_DEBUG_SRV						CONCAT( t, HLSL_BLOCK_DEBUG_SLOT )

#define HLSL_CULL_STATS_SRV							CONCAT( t, HLSL_CULL_STATS_SLOT )
#define HLSL_CULL_DEBUG_SRV							CONCAT( t, HLSL_CULL_DEBUG_SLOT )

#define HLSL_TRACE_CELLS_SRV						CONCAT( t, HLSL_TRACE_CELLS_SLOT )
#define HLSL_TRACE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_TRACE_INDIRECT_ARGS_SLOT )
#define HLSL_TRACE_STATS_SRV						CONCAT( t, HLSL_TRACE_STATS_SLOT )
#define HLSL_TRACE_DEBUG_SRV						CONCAT( t, HLSL_TRACE_DEBUG_SLOT )

#define HLSL_PIXEL_DEBUG_SRV						CONCAT( t, HLSL_PIXEL_DEBUG_SLOT )

#define HLSL_GENERIC_ALBEDO_SRV						CONCAT( t, HLSL_GENERIC_ALBEDO_SLOT )
#define HLSL_GENERIC_ALPHA_SRV						CONCAT( t, HLSL_GENERIC_ALPHA_SLOT )

#define HLSL_SURFACE_UAV							CONCAT( u, HLSL_SURFACE_SLOT )
#define HLSL_LCOLOR_UAV								CONCAT( u, HLSL_LCOLOR_SLOT )
#define HLSL_RCOLOR_UAV								CONCAT( u, HLSL_RCOLOR_SLOT )

#define HLSL_SAMPLE_UAV								CONCAT( u, HLSL_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_UAV						CONCAT( u, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_BLOCK_POS_UAV							CONCAT( u, HLSL_BLOCK_POS_SLOT )
#define HLSL_BLOCK_DATA_UAV							CONCAT( u, HLSL_BLOCK_DATA_SLOT )
#define HLSL_BLOCK_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_BLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_BLOCK_CELL_ITEM_UAV					CONCAT( u, HLSL_BLOCK_CELL_ITEM_SLOT )
#define HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV			CONCAT( u, HLSL_BLOCK_FIRST_CELL_ITEM_ID_SLOT )
#define HLSL_BLOCK_CELL_ITEM_COUNT_UAV				CONCAT( u, HLSL_BLOCK_CELL_ITEM_COUNT_SLOT )
#define HLSL_BLOCK_DEBUG_UAV						CONCAT( u, HLSL_BLOCK_DEBUG_SLOT )

#define HLSL_CULL_STATS_UAV							CONCAT( u, HLSL_CULL_STATS_SLOT )
#define HLSL_CULL_DEBUG_UAV							CONCAT( u, HLSL_CULL_DEBUG_SLOT )

#define HLSL_TRACE_CELLS_UAV						CONCAT( u, HLSL_TRACE_CELLS_SLOT )
#define HLSL_TRACE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_TRACE_INDIRECT_ARGS_SLOT )
#define HLSL_TRACE_STATS_UAV						CONCAT( u, HLSL_TRACE_STATS_SLOT )
#define HLSL_TRACE_DEBUG_UAV						CONCAT( u, HLSL_TRACE_DEBUG_SLOT )

#define HLSL_PIXEL_DEBUG_UAV						CONCAT( u, HLSL_PIXEL_DEBUG_SLOT )

#define HLSL_GRID_MACRO_SHIFT						8
#define HLSL_GRID_MACRO_2XSHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_WIDTH						(1 << HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_MASK						(HLSL_GRID_MACRO_WIDTH-1)

#define HLSL_GRID_BLOCK_COUNT						(1 << HLSL_GRID_MACRO_3XSHIFT)
#define HLSL_GRID_BLOCK_SHIFT						2
#define HLSL_GRID_BLOCK_2XSHIFT						(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_3XSHIFT						(HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_WIDTH						(1 << HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_BLOCK_MASK						(HLSL_GRID_BLOCK_WIDTH-1)
#define HLSL_GRID_BLOCK_INVALID						uint( -1 )
#define HLSL_GRID_BLOCK_SETTING						uint( -2 )

#define HLSL_GRID_BLOCK_CELL_COUNT					(1 << HLSL_GRID_BLOCK_3XSHIFT)
#define HLSL_GRID_BLOCK_CELL_POS_MASK				(HLSL_GRID_BLOCK_CELL_COUNT-1)
#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFFFFFFFF

#define HLSL_GRID_SHIFT								(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_WIDTH								(1 << HLSL_GRID_SHIFT)
#define HLSL_GRID_MASK								(HLSL_GRID_WIDTH-1)
#define HLSL_GRID_INV_WIDTH							(1.0f / HLSL_GRID_WIDTH)
#define HLSL_GRID_HALF_WIDTH						(HLSL_GRID_WIDTH >> 1)
#define HLSL_GRID_QUARTER_WIDTH						(HLSL_GRID_WIDTH >> 2)

#define HLSL_SAMPLE_THREAD_GROUP_SIZE				128
#define HLSL_OCTREE_THREAD_GROUP_SIZE				64
#define HLSL_BLOCK_THREAD_GROUP_SIZE				64
#define	HLSL_MIP_MAX_COUNT							16
#define HLSL_NODE_CREATED							0x80000000
#define HLSL_BUCKET_COUNT							5
#define HLSL_CELL_SUPER_SAMPLING_WIDTH				3
#define HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ			(HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH)
#define HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE			(HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH)
#define HLSL_CELL_SUPER_SAMPLING_WIDTH				3
#define HLSL_PIXEL_SUPER_SAMPLING_WIDTH				1
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT		2
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT		(1 << HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_SHIFT)
#define HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_MASK		(HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT-1)
#define HLSL_CELL_ITEM_PAGE_COUNT					16
#define HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT			(HLSL_CELL_ITEM_PER_PAGE_PER_PIXEL_COUNT * HLSL_CELL_ITEM_PAGE_COUNT)

CBUFFER( CBBasic, 0 )
{
	row_major	matrix	c_basicObjectToView;
	row_major	matrix	c_basicViewToProj;
	row_major	matrix	c_basicObjectToProj;
};

CBUFFER( CBGeneric, 1 )
{
	row_major	matrix	c_genericObjectToWorld;
	row_major	matrix	c_genericWorldToView;
	row_major	matrix	c_genericViewToProj;
	
	float3				c_genericDiffuse;
	int					c_genericPad0;

	int					c_genericUseAlbedo;
	int					c_genericUseAlpha;	
	int					c_genericPad1;
	int					c_genericPad2;
	
};

CBUFFER( CBSample, 2 )
{
	float				c_sampleDepthLinearScale;
	float				c_sampleDepthLinearBias;
	float2				c_sampleInvCubeSize;
	float3				c_sampleOffset;
	uint				c_sampleFaceID;
	float4				c_sampleMipBoundariesA;
	float4				c_sampleMipBoundariesB;
	float4				c_sampleMipBoundariesC;
	float4				c_sampleMipBoundariesD;
	float4				c_sampleInvGridScales[HLSL_MIP_MAX_COUNT];	
};

CBUFFER( CBOctree, 3 )
{
	uint				c_octreeCurrentLevel;
	uint				c_octreeCurrentBucket;
	float				c_octreePad0;
	float				c_octreePad1;
};

CBUFFER( CBBlock, 4 )
{
	row_major	matrix	c_blockObjectToView;
	row_major	matrix	c_blockViewToObject;
	row_major	matrix	c_blockViewToProj;
	
	float4				c_blockGridScales[HLSL_MIP_MAX_COUNT];
	
	float3				c_blockCenter;
	uint				c_blockShowVoxel;
	
	float2				c_blockFrameSize;
	float2				c_blockMultiSampledFrameSize;
	
	float2				c_blockScreenToClipScale;
	float2				c_blockMultiSampledScreenToClipScale;
	
	float2				c_blockScreenToClipOffset;	
	float				c_blockZNear;
	float				c_blockUnused;

	float3				c_blockRayDirBase;
	float				c_blockPad0;
	float3				c_blockRayDirUp;
	float				c_blockPad1;
	float3				c_blockRayDirRight;
	float				c_blockPad2;

	uint				c_blockShowMip;
	uint				c_blockShowOverdraw;	
	uint				c_blockUseOccupancy;
	uint				c_blockProfileTrace;
};

CBUFFER( CBPixel, 5 )
{
	uint2				c_pixelFrameSize;
	float2				c_pixelPad3;
	float3				c_pixelBackColor;
	float				c_pixelPad4;
#if HLSL_DEBUG_PIXEL == 1	
	uint				c_pixelMode;
	uint				c_pixelDebug;
	uint2				c_pixelDebugCoords;	
#endif // #if HLSL_DEBUG_PIXEL == 1
};

CBUFFER( CBCompose, 6 )
{
	uint				c_composeFrameWidth;
	uint3				c_unused;
};

struct Sample
{
	uint row0;
	uint row1;
	uint row2;
};

struct OctreeLeaf
{
	uint x9_r23;
	uint y9_g23;
	uint z9_b23;
	uint x2y2z2_mip4_count15;
	uint occupancy27;
};

#if HLSL_PIXEL_SUPER_SAMPLING_WIDTH == 1

struct BlockCellItem
{
	float	depth;
	uint	r8g8b8_hitMask8;
};

#endif // #if HLSL_PIXEL_SUPER_SAMPLING_WIDTH == 1

#if HLSL_PIXEL_SUPER_SAMPLING_WIDTH == 3

struct BlockCellItem
{
	uint	r8g8b8a8;
	uint	depth23_occupancy9;
	uint	nextID;
};

#endif // #if HLSL_PIXEL_SUPER_SAMPLING_WIDTH == 3

struct BlockCullStats 
{
	uint	blockInputCount;
	uint	blockPassedCount;
	uint	cellOutputCount;
};

struct BlockTraceStats 
{
	uint	cellInputCount;
	uint	cellProcessedCount;
	uint	pixelSampleCount;
	uint	cellItemCount;
	uint	cellItemMaxCountPerPixel;
};

#if HLSL_DEBUG_PIXEL == 1

struct PixelBlendDebugBuffer
{
	BlockCellItem cellItems[16];
	uint pixelRank;
	uint itemCount;
};

#endif // #if HLSL_DEBUG_PIXEL == 1

#define sample_groupCountX_offset							0
#define sample_groupCountY_offset							1
#define sample_groupCountZ_offset							2
#define sample_count_offset									3
#if HLSL_DEBUG_COLLECT == 1
#define sample_out_offset									4
#define sample_error_offset									5
#define sample_pixelCount_offset							6
#define sample_pixelSampleCount_offset						7
#if 0
#define sample_occupancy_offset								8
#define sample_cellCoords_offset( ID )						(9 + (ID))
#define sample_all_offset									(sample_cellCoords_offset( 144 ) + 1)
#else
#define sample_all_offset									8
#endif
#else
#define sample_all_offset									4
#endif // #if HLSL_DEBUG_COLLECT == 1

#define sample_groupCountX									sampleIndirectArgs[sample_groupCountX_offset] // ThreadGroupCountX
#define sample_groupCountY									sampleIndirectArgs[sample_groupCountY_offset] // ThreadGroupCountY
#define sample_groupCountZ									sampleIndirectArgs[sample_groupCountZ_offset] // ThreadGroupCountZ
#if HLSL_DEBUG_COLLECT == 1
#define sample_out											sampleIndirectArgs[sample_out_offset]
#define sample_error										sampleIndirectArgs[sample_error_offset]
#define sample_pixelCount									sampleIndirectArgs[sample_pixelCount_offset]
#define sample_pixelSampleCount								sampleIndirectArgs[sample_pixelSampleCount_offset]
#if 0
#define sample_occupancy									sampleIndirectArgs[sample_occupancy_offset]
#define sample_cellCoords( ID )								sampleIndirectArgs[sample_cellCoords_offset( ID )]
#endif
#endif // #if HLSL_DEBUG_COLLECT == 1
#define sample_count										sampleIndirectArgs[sample_count_offset]

#define octree_leafGroupCountX_offset						0
#define octree_leafGroupCountY_offset						1
#define octree_leafGroupCountZ_offset						2
#define octree_leafCount_offset								3
#define octree_nodeCount_offset								4
#define octree_all_offset									5

#define octree_leafGroupCountX								octreeIndirectArgs[octree_leafGroupCountX_offset] // ThreadGroupCountX
#define octree_leafGroupCountY								octreeIndirectArgs[octree_leafGroupCountY_offset] // ThreadGroupCountY
#define octree_leafGroupCountZ								octreeIndirectArgs[octree_leafGroupCountZ_offset] // ThreadGroupCountZ
#define octree_leafCount									octreeIndirectArgs[octree_leafCount_offset]
#define octree_nodeCount									octreeIndirectArgs[octree_nodeCount_offset]

#define block_groupCountX_offset( BUCKET )					(BUCKET * 3 + 0)
#define block_groupCountY_offset( BUCKET )					(BUCKET * 3 + 1)
#define block_groupCountZ_offset( BUCKET )					(BUCKET * 3 + 2)

#define block_count_offset(	BUCKET )						(block_groupCountZ_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_posOffset_offset( BUCKET )					(block_count_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_dataOffset_offset( BUCKET )					(block_posOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_cellCount_offset( BUCKET )					(block_dataOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET)

#define block_uniqueOccupancyCount_offset( BUCKET )			(block_cellCount_offset( HLSL_BUCKET_COUNT ) + BUCKET)
#define block_uniqueOccupancyMax_offset( BUCKET )			(block_uniqueOccupancyCount_offset( HLSL_BUCKET_COUNT ) + BUCKET)
#define block_slotOccupancyCount_offset( BUCKET )			(block_uniqueOccupancyMax_offset( HLSL_BUCKET_COUNT ) + BUCKET)

#define block_all_offset									block_slotOccupancyCount_offset( HLSL_BUCKET_COUNT )

#define block_groupCountX( BUCKET )							blockIndirectArgs[block_groupCountX_offset( BUCKET )]
#define block_groupCountY( BUCKET )							blockIndirectArgs[block_groupCountY_offset( BUCKET )]
#define block_groupCountZ( BUCKET )							blockIndirectArgs[block_groupCountZ_offset( BUCKET )]

#define block_count( BUCKET )								blockIndirectArgs[block_count_offset( BUCKET )]
#define block_posOffset( BUCKET )							blockIndirectArgs[block_posOffset_offset( BUCKET )]
#define block_dataOffset( BUCKET )							blockIndirectArgs[block_dataOffset_offset( BUCKET )]
#define block_cellCount( BUCKET )							blockIndirectArgs[block_cellCount_offset( BUCKET )]

#define block_uniqueOccupancyCount( BUCKET )				blockIndirectArgs[block_uniqueOccupancyCount_offset( BUCKET )]
#define block_uniqueOccupancyMax( BUCKET )					blockIndirectArgs[block_uniqueOccupancyMax_offset( BUCKET )]
#define block_slotOccupancyCount( BUCKET )					blockIndirectArgs[block_slotOccupancyCount_offset( BUCKET )]

#if HLSL_ENCODE_DATA == 1

#define trace_cellGroupCountX_offset						0
#define trace_cellGroupCountY_offset						1
#define trace_cellGroupCountZ_offset						2
#define trace_cellCount_offset								3
#define trace_all_offset									4

#define trace_cellGroupCountX								traceIndirectArgs[trace_cellGroupCountX_offset]
#define trace_cellGroupCountY								traceIndirectArgs[trace_cellGroupCountY_offset]
#define trace_cellGroupCountZ								traceIndirectArgs[trace_cellGroupCountZ_offset]
#define trace_cellCount										traceIndirectArgs[trace_cellCount_offset]

#else

#define trace_cellGroupCountX_offset( BUCKET )				(BUCKET * 3 + 0)
#define trace_cellGroupCountY_offset( BUCKET )				(BUCKET * 3 + 1)
#define trace_cellGroupCountZ_offset( BUCKET )				(BUCKET * 3 + 2)
#define trace_blockOffset_offset( BUCKET )					(trace_cellGroupCountZ_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define trace_blockCount_offset( BUCKET )					(trace_blockOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define trace_all_offset									trace_blockCount_offset( HLSL_BUCKET_COUNT )

#define trace_cellGroupCountX( BUCKET )						traceIndirectArgs[trace_cellGroupCountX_offset( BUCKET )]
#define trace_cellGroupCountY( BUCKET )						traceIndirectArgs[trace_cellGroupCountY_offset( BUCKET )]
#define trace_cellGroupCountZ( BUCKET )						traceIndirectArgs[trace_cellGroupCountZ_offset( BUCKET )]
#define trace_blockOffset( BUCKET )							traceIndirectArgs[trace_blockOffset_offset( BUCKET )]
#define trace_blockCount( BUCKET )							traceIndirectArgs[trace_blockCount_offset( BUCKET )]

#endif

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__