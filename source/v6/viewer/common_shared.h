/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define CONCAT( X, Y )								X ## Y
#define GROUP_COUNT( C, S )							(((C) + (S) - 1)) / (S)

#define HLSL_DEBUG_COLLECT							1
#define HLSL_DEBUG_BLOCK							1
#define HLSL_DEBUG_PIXEL							1
#define HLSL_TRACE_USE_ALIGNED_QUAD					1

#define HLSL_TRILINEAR_SLOT							0

#define HLSL_COLOR_SLOT								0
#define HLSL_UV_SLOT								1
#define HLSL_DEPTH_SLOT								2
#define HLSL_PIXEL_SAMPLE_POSITION_SLOT				3

#define HLSL_SAMPLE_SLOT							2
#define HLSL_SAMPLE_INDIRECT_ARGS_SLOT				3
#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT			6
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT			7
#define HLSL_OCTREE_LEAF_SLOT						8
#define HLSL_OCTREE_INDIRECT_ARGS_SLOT				9
#define HLSL_BLOCK_COLOR_SLOT						10
#define HLSL_BLOCK_INDIRECT_ARGS_SLOT				11
#define HLSL_BLOCK_CELL_ITEM_SLOT					12
#define HLSL_BLOCK_FIRST_CELL_ITEM_ID_SLOT			13
#define HLSL_BLOCK_CONTEXT_SLOT						14
#define HLSL_PIXEL_COLOR_SLOT						15
#define HLSL_PIXEL_DEBUG_SLOT						16

#define HLSL_GENERIC_ALBEDO_SLOT					2
#define HLSL_GENERIC_ALPHA_SLOT						3

#define HLSL_TRILINEAR_SAMPLER						CONCAT( s, HLSL_TRILINEAR_SLOT )

#define HLSL_COLOR_SRV								CONCAT( t, HLSL_COLOR_SLOT )
#define HLSL_UV_SRV									CONCAT( t, HLSL_UV_SLOT )
#define HLSL_DEPTH_SRV								CONCAT( t, HLSL_DEPTH_SLOT )
#define HLSL_PIXEL_SAMPLE_POSITION_SRV				CONCAT( t, HLSL_PIXEL_SAMPLE_POSITION_SLOT )

#define HLSL_SAMPLE_SRV								CONCAT( t, HLSL_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_SRV			CONCAT( t, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_SRV						CONCAT( t, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_BLOCK_COLOR_SRV						CONCAT( t, HLSL_BLOCK_COLOR_SLOT )
#define HLSL_BLOCK_INDIRECT_ARGS_SRV				CONCAT( t, HLSL_BLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_BLOCK_CELL_ITEM_SRV					CONCAT( t, HLSL_BLOCK_CELL_ITEM_SLOT )
#define HLSL_BLOCK_FIRST_CELL_ITEM_ID_SRV			CONCAT( t, HLSL_BLOCK_FIRST_CELL_ITEM_ID_SLOT )
#define HLSL_BLOCK_CONTEXT_SRV						CONCAT( t, HLSL_BLOCK_CONTEXT_SLOT )

#define HLSL_PIXEL_COLOR_SRV						CONCAT( t, HLSL_PIXEL_COLOR_SLOT )
#define HLSL_PIXEL_DEBUG_SRV						CONCAT( t, HLSL_PIXEL_DEBUG_SLOT )

#define HLSL_GENERIC_ALBEDO_SRV						CONCAT( t, HLSL_GENERIC_ALBEDO_SLOT )
#define HLSL_GENERIC_ALPHA_SRV						CONCAT( t, HLSL_GENERIC_ALPHA_SLOT )

#define HLSL_COLOR_UAV								CONCAT( u, HLSL_COLOR_SLOT )
#define HLSL_PIXEL_SAMPLE_POSITION_UAV				CONCAT( u, HLSL_PIXEL_SAMPLE_POSITION_SLOT )

#define HLSL_SAMPLE_UAV								CONCAT( u, HLSL_SAMPLE_SLOT )
#define HLSL_SAMPLE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_SAMPLE_INDIRECT_ARGS_SLOT )

#define HLSL_OCTREE_SAMPLE_NODE_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT )
#define HLSL_OCTREE_FIRST_CHILD_OFFSET_UAV			CONCAT( u, HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT )
#define HLSL_OCTREE_LEAF_UAV						CONCAT( u, HLSL_OCTREE_LEAF_SLOT )
#define HLSL_OCTREE_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_OCTREE_INDIRECT_ARGS_SLOT )

#define HLSL_BLOCK_COLOR_UAV						CONCAT( u, HLSL_BLOCK_COLOR_SLOT )
#define HLSL_BLOCK_INDIRECT_ARGS_UAV				CONCAT( u, HLSL_BLOCK_INDIRECT_ARGS_SLOT )
#define HLSL_BLOCK_CELL_ITEM_UAV					CONCAT( u, HLSL_BLOCK_CELL_ITEM_SLOT )
#define HLSL_BLOCK_FIRST_CELL_ITEM_ID_UAV			CONCAT( u, HLSL_BLOCK_FIRST_CELL_ITEM_ID_SLOT )
#define HLSL_BLOCK_CONTEXT_UAV						CONCAT( u, HLSL_BLOCK_CONTEXT_SLOT )

#define HLSL_PIXEL_COLOR_UAV						CONCAT( u, HLSL_PIXEL_COLOR_SLOT )
#define HLSL_PIXEL_DEBUG_UAV						CONCAT( u, HLSL_PIXEL_DEBUG_SLOT )

#define HLSL_GRID_MACRO_SHIFT						8
#define HLSL_GRID_MACRO_2XSHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)
#define HLSL_GRID_MACRO_3XSHIFT						(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT + HLSL_GRID_MACRO_SHIFT)w
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
#define HLSL_GRID_BLOCK_CELL_EMPTY					0xFF

#define HLSL_GRID_SHIFT								(HLSL_GRID_MACRO_SHIFT + HLSL_GRID_BLOCK_SHIFT)
#define HLSL_GRID_WIDTH								(1 << HLSL_GRID_SHIFT)
#define HLSL_GRID_MASK								(HLSL_GRID_WIDTH-1)
#define HLSL_GRID_INV_WIDTH							(1.0f / HLSL_GRID_WIDTH)
#define HLSL_GRID_HALF_WIDTH						(HLSL_GRID_WIDTH >> 1)
#define HLSL_GRID_QUARTER_WIDTH						(HLSL_GRID_WIDTH >> 2)

#define HLSL_SAMPLE_THREAD_GROUP_SIZE				128
#define HLSL_OCTREE_THREAD_GROUP_SIZE				64
#define HLSL_BLOCK_THREAD_GROUP_SIZE				128
#define HLSL_TRACE_THREAD_GROUP_SIZE				64
#define	HLSL_MIP_MAX_COUNT							8
#define HLSL_NODE_CREATED							0x80000000
#define HLSL_BUCKET_COUNT							5
#define HLSL_PIXEL_SUPER_SAMPLING_WIDTH				3
#define HLSL_PIXEL_SUPER_SAMPLING_WIDTH_SQ			(HLSL_PIXEL_SUPER_SAMPLING_WIDTH * HLSL_PIXEL_SUPER_SAMPLING_WIDTH)
#define HLSL_PIXEL_SUPER_SAMPLING_WIDTH_CUBE			(HLSL_PIXEL_SUPER_SAMPLING_WIDTH * HLSL_PIXEL_SUPER_SAMPLING_WIDTH * HLSL_PIXEL_SUPER_SAMPLING_WIDTH)
#define	HLSL_PIXEL_MULTISAMPLE_COUNT				8
#define	HLSL_PIXEL_MULTISAMPLE_WIDTH				1024
#define HLSL_COUNT									2

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
	float2				c_blockScreenToClipOffset;
	
	float				c_blockZNear;
	uint				c_blockShowMip;
	uint				c_blockShowOverdraw;	
	uint				c_blockUseOccupancy;
};

CBUFFER( CBPixel, 5 )
{
	float				c_pixelDepthLinearScale;
	float				c_pixelDepthLinearBias;
	uint2				c_pixelFrameSize;
	float4				c_pixelInvCellSizes[HLSL_MIP_MAX_COUNT];
	float3				c_pixelBackColor;
	float				c_pixelPad2;
#if HLSL_DEBUG_PIXEL == 1	
	uint				c_pixelMode;
	uint				c_pixelDebug;
	uint2				c_pixelDebugCoords;	
#endif // #if HLSL_DEBUG_PIXEL == 1
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

struct BlockCellItem
{
	uint	r8g8b8a8;
	uint	u8v8w8h8;
	float	depth;
	uint	nextID;
};

struct BlockContext
{
	uint cellItemCount;
#if HLSL_DEBUG_BLOCK == 1
	
	float2 screenPos;
	float2 screenRadius;
	float2 multiSampledPixelPos;
	float2 multiSampledPixelRadius;
	uint2 multiSampledMinPixelCoords;
	uint2 multiSampledMaxPixelCoords;
	
	bool cull;
	
	uint2 minPixelCoords;
	uint2 multiSampledMinPixelBase;	
	uint2 multiSampledMinPixelOffset;
	uint2 multiSampledSize;
	uint multiSampledPixelCount;	
	uint jobCount;

	uint jobBlockID;
	uint jobWidth;
	uint jobOffset;
	uint jobMip;
	uint jobPixelID;
	uint jobPixelBaseX;
	uint jobPixelBaseY;
	uint jobPixelOffsetX;
	uint jobPixelOffsetY;
	float lineCount;
	float jobPixelX;
	float jobPixelY;		
		
	float3 gridCenter;
	float gridScale;
	uint gridOccupancy;

	float2 jobMultiSampledScreenPos;
	float3 rayEndVS;
	float3 rayOrgWS;	
	float3 rayEndWS;
	float3 rayDir;
	float3 rayInvDir;
	float3 alpha;
	float3 beta;	
	float3 t0;
	float3 t1;
	float3 tMin;
	float3 tMax;
	float tIn;
	float tOut;

	float scale;
	float offset;	
	float3 pIn;
	float3 coordIn;			
	int3 coords;
	float3 tCur;
	float3 tDelta;		
	int3 step;
	uint2 jobPixelCoord;

#endif // HLSL_DEBUG_BLOCK == 1
};

#if HLSL_DEBUG_PIXEL == 1

struct PixelDebugLayer
{
	float4 colorAndDepth;
	float2 uv;	
	float2 wh;
	int2 uvMin;
	int2 uvMax;
};

struct PixelDebugPoint
{
	PixelDebugLayer layers[4];
	uint layerCount;
};

struct PixelDebugBuffer
{
	PixelDebugPoint points[3][3];
	float3 colorBuffer[HLSL_PIXEL_SUPER_SAMPLING_WIDTH][HLSL_PIXEL_SUPER_SAMPLING_WIDTH];
	float depthBuffer[HLSL_PIXEL_SUPER_SAMPLING_WIDTH][HLSL_PIXEL_SUPER_SAMPLING_WIDTH];
};

struct PixelBlendDebugLayer
{
	float4 colorAndDepth;
	uint occupancy;
};

struct PixelBlendDebugBuffer
{
	PixelBlendDebugLayer layers[4];
	uint layerCount;
	float3 colorBuffer[HLSL_PIXEL_SUPER_SAMPLING_WIDTH][HLSL_PIXEL_SUPER_SAMPLING_WIDTH];
	float depthBuffer[HLSL_PIXEL_SUPER_SAMPLING_WIDTH][HLSL_PIXEL_SUPER_SAMPLING_WIDTH];
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

#define block_vertexCountPerInstance_offset( BUCKET )		(BUCKET * 4 + 0)
#define block_renderInstanceCount_offset( BUCKET )			(BUCKET * 4 + 1)
#define block_startVertexLocation_offset( BUCKET )			(BUCKET * 4 + 2)
#define block_renderInstanceLocation_offset( BUCKET )		(BUCKET * 4 + 3)

#define block_indexCountPerInstance_offset( BUCKET )		(block_renderInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 0))
#define block_instanceCount_offset( BUCKET )				(block_renderInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 1))
#define block_startIndexLocation_offset( BUCKET )			(block_renderInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 2))
#define block_baseVertexLocation_offset( BUCKET )			(block_renderInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 3))
#define block_startInstanceLocation_offset( BUCKET )		(block_renderInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 4))

#define block_indexCountPerInstance2_offset( BUCKET )		(block_startInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 0))
#define block_instanceCount2_offset( BUCKET )				(block_startInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 1))
#define block_startIndexLocation2_offset( BUCKET )			(block_startInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 2))
#define block_baseVertexLocation2_offset( BUCKET )			(block_startInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 3))
#define block_startInstanceLocation2_offset( BUCKET )		(block_startInstanceLocation_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 5 + 4))

#define block_cellGroupCountX_offset( BUCKET )				(block_startInstanceLocation2_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 3 + 0))
#define block_cellGroupCountY_offset( BUCKET )				(block_startInstanceLocation2_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 3 + 1))
#define block_cellGroupCountZ_offset( BUCKET )				(block_startInstanceLocation2_offset( HLSL_BUCKET_COUNT ) + (BUCKET * 3 + 2))

#define block_count_offset(	BUCKET )						(block_cellGroupCountZ_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_packedOffset_offset( BUCKET )					(block_count_offset( HLSL_BUCKET_COUNT ) + BUCKET + 1)
#define block_cellCount_offset( BUCKET )					(block_packedOffset_offset( HLSL_BUCKET_COUNT ) + BUCKET)
#define block_all_offset									block_cellCount_offset( HLSL_BUCKET_COUNT )

#define block_vertexCountPerInstance( BUCKET )				blockIndirectArgs[block_vertexCountPerInstance_offset( BUCKET )]
#define block_renderInstanceCount( BUCKET )					blockIndirectArgs[block_renderInstanceCount_offset( BUCKET )]
#define block_startVertexLocation( BUCKET )					blockIndirectArgs[block_startVertexLocation_offset( BUCKET )]
#define block_renderInstanceLocation( BUCKET )				blockIndirectArgs[block_renderInstanceLocation_offset( BUCKET )]

#define block_indexCountPerInstance( BUCKET )				blockIndirectArgs[block_indexCountPerInstance_offset( BUCKET )]
#define block_instanceCount( BUCKET )						blockIndirectArgs[block_instanceCount_offset( BUCKET )]
#define block_startIndexLocation( BUCKET )					blockIndirectArgs[block_startIndexLocation_offset( BUCKET )]
#define block_baseVertexLocation( BUCKET )					blockIndirectArgs[block_baseVertexLocation_offset( BUCKET )]
#define block_startInstanceLocation( BUCKET )				blockIndirectArgs[block_startInstanceLocation_offset( BUCKET )]

#define block_indexCountPerInstance2( BUCKET )				blockIndirectArgs[block_indexCountPerInstance2_offset( BUCKET )]
#define block_instanceCount2( BUCKET )						blockIndirectArgs[block_instanceCount2_offset( BUCKET )]
#define block_startIndexLocation2( BUCKET )					blockIndirectArgs[block_startIndexLocation2_offset( BUCKET )]
#define block_baseVertexLocation2( BUCKET )					blockIndirectArgs[block_baseVertexLocation2_offset( BUCKET )]
#define block_startInstanceLocation2( BUCKET )				blockIndirectArgs[block_startInstanceLocation2_offset( BUCKET )]

#define block_cellGroupCountX( BUCKET )						blockIndirectArgs[block_cellGroupCountX_offset( BUCKET )]
#define block_cellGroupCountY( BUCKET )						blockIndirectArgs[block_cellGroupCountY_offset( BUCKET )]
#define block_cellGroupCountZ( BUCKET )						blockIndirectArgs[block_cellGroupCountZ_offset( BUCKET )]

#define block_count( BUCKET )								blockIndirectArgs[block_count_offset( BUCKET )]
#define block_packedOffset( BUCKET )						blockIndirectArgs[block_packedOffset_offset( BUCKET )]
#define block_cellCount( BUCKET )							blockIndirectArgs[block_cellCount_offset( BUCKET )]

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__