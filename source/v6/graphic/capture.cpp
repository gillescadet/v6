/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/codec/codec.h>
#include <v6/core/plot.h>
#include <v6/graphic/capture.h>
#include <v6/graphic/capture_shaders.h>
#include <v6/graphic/capture_shared.h>

#define CAPTURE_DEBUG 0

BEGIN_V6_NAMESPACE

struct GPUCaptureResources_s
{
	GPUConstantBuffer_s			cbCollect;
	GPUConstantBuffer_s			cbOctree;
	
	GPUBuffer_s					samples;
	GPUBuffer_s					sampleIndirectArgs;
	GPUBuffer_s					sampleNodeOffsets;
	GPUBuffer_s					firstChildOffsets;
	ID3D11UnorderedAccessView*	firstChildOffsetsLimitedUAV;
	GPUBuffer_s					leaves;
	GPUBuffer_s					octreeIndirectArgs;
	GPUBuffer_s					blockPos;
	GPUBuffer_s					blockData;
	GPUBuffer_s					blockIndirectArgs;

	GPUCompute_s				computeCollect;
	GPUCompute_s				computeBuildInner;
	GPUCompute_s				computeBuildLeaf;
	GPUCompute_s				computeFillLeaf;
	GPUCompute_s				computePackColor;
};

extern ID3D11Device*							g_device;
extern ID3D11DeviceContext*						g_deviceContext;

#if HLSL_CELL_SUPER_SAMPLING_WIDTH > 1
static const float AVERAGE_SAMPLE_PER_PIXEL		= 0.25f * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
#else
static const float AVERAGE_SAMPLE_PER_PIXEL		= 1.0f;
#endif
static const float AVERAGE_LAYER_COUNT			= 2.0f;

static GPUCaptureResources_s					s_gpuCaptureResources;
static bool										s_gpuCaptureResourcesCreated = false;

#if CAPTURE_DEBUG == 1
Plot_s s_plot;
#endif

static void ClearNode( CaptureContext_s* captureContext )
{
	GPUCaptureResources_s* res = captureContext->res;

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( res->octreeIndirectArgs.uav, values );
	g_deviceContext->ClearUnorderedAccessViewUint( res->firstChildOffsetsLimitedUAV, values );
}

static void Collect( const CaptureContext_s* captureContext, const Vec3* samplePos, const Vec3 basis[3], ID3D11ShaderResourceView* colorSRV, ID3D11ShaderResourceView* depthSRV )
{
	GPUEvent_Begin( "Collect" );

	GPUCaptureResources_s* res = captureContext->res;
	
	// Clear
	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( res->sampleIndirectArgs.uav, values );

	// Update buffers

	V6_ASSERT( captureContext->desc.gridMacroShift > 0 );
	const u32 gridMacroHalfWidth = 1 << (captureContext->desc.gridMacroShift-1);
	const u32 gridWidth = 1 << (captureContext->desc.gridMacroShift + 2);
	const u32 cubeWidth = gridWidth * HLSL_CELL_SUPER_SAMPLING_WIDTH;

	{
		float gridScales[CODEC_MIP_MAX_COUNT];
		Vec3 gridCenters[CODEC_MIP_MAX_COUNT];
		float gridScale = captureContext->desc.gridScaleMin;
		for ( u32 gridID = 0;  gridID < CODEC_MIP_MAX_COUNT; ++gridID )
		{
			gridScales[gridID] = gridScale;
			gridCenters[gridID] = Codec_ComputeGridCenter( &captureContext->frameState.origin, gridScale, gridMacroHalfWidth );
			if ( gridScale < captureContext->desc.gridScaleMax )
				gridScale *= 2;
		}

		hlsl::CBSample* cbSample = (hlsl::CBSample*)GPUConstantBuffer_MapWrite( &res->cbCollect );

		cbSample->c_sampleRight = Vec4_Make( &basis[0], 0.0f );
		cbSample->c_sampleUp = Vec4_Make( &basis[1], 0.0f );
		cbSample->c_sampleForward = Vec4_Make( &basis[2], 0.0f );
		cbSample->c_sampleDepthLinearScale = captureContext->desc.depthLinearScale; // -1.0f / ZNEAR;
		cbSample->c_sampleDepthLinearBias = captureContext->desc.depthLinearBias; // 1.0f / ZNEAR;
		cbSample->c_sampleGridWidth = gridWidth;
		cbSample->c_sampleInvCubeSize = 1.0f / cubeWidth;
		cbSample->c_samplePos = *samplePos;
		for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleMipBoundaries[gridID] = Vec4_Make( &gridCenters[gridID], gridScales[gridID] );
		for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleInvGridScales[gridID] = Vec4_Make( 1.0f / gridScales[gridID], 0.0f, 0.0f , 0.0f );

		GPUConstantBuffer_UnmapWrite( &res->cbCollect );
	}

	// Set
	g_deviceContext->CSSetConstantBuffers( hlsl::CBSampleSlot, 1, &res->cbCollect.buf );
	g_deviceContext->CSSetShaderResources( HLSL_CAPTURE_COLOR_SLOT, 1, &colorSRV );
	g_deviceContext->CSSetShaderResources( HLSL_CAPTURE_DEPTH_SLOT, 1, &depthSRV );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, &res->samples.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &res->sampleIndirectArgs.uav, nullptr );
	g_deviceContext->CSSetShader( res->computeCollect.m_computeShader, nullptr, 0 );

	// Dispatch
	const u32 cubeGroupCount = gridWidth >> 3;
	g_deviceContext->Dispatch( cubeGroupCount, cubeGroupCount, 1 );

	// Unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_CAPTURE_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_CAPTURE_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();

	if ( captureContext->desc.logReadBack )
	{
		// Read back
		const u32* collectedIndirectArgs = (u32*)GPUBuffer_MapReadBack( &res->sampleIndirectArgs );
		
		V6_MSG( "\n" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_groupCountX_offset], "groupCountX" );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountY_offset] == 1 );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountZ_offset] == 1 );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_count_offset], "count" );
		V6_ASSERT( collectedIndirectArgs[sample_count_offset] <= captureContext->resSampleCount );
#if HLSL_DEBUG_COLLECT == 1
		ReadBack_Log( "sample", collectedIndirectArgs[sample_pixelCount_offset], "pixelCount" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_pixelSampleCount_offset], "pixelSampleCount" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_out_offset], "out" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_error_offset], "error" );
#if 0
		ReadBack_Log( "sample", collectedIndirectArgs[sample_occupancy_offset], "occupancy" );
		for ( u32 sampleID = 0; sampleID < 144; ++sampleID )
		{
			u32 value = collectedIndirectArgs[sample_cellCoords_offset( sampleID )];
			ReadBack_Log( "sample", *((float*)&value), "cellCoords.x" );
		}
#endif
		V6_ASSERT( collectedIndirectArgs[sample_error_offset] == 0 );
#endif // #if HLSL_DEBUG_COLLECT == 1

		GPUBuffer_UnmapReadBack( &res->sampleIndirectArgs );
	}

#if CAPTURE_DEBUG == 1
	{
		const u32* collectedIndirectArgs = (u32*)GPUBuffer_MapReadBack( &res->sampleIndirectArgs );
		const u32 sampleCount = collectedIndirectArgs[sample_count_offset];
		GPUBuffer_UnmapReadBack( &res->sampleIndirectArgs );

		float gridScales[CODEC_MIP_MAX_COUNT];
		float halfCellSizes[CODEC_MIP_MAX_COUNT];
		Vec3 gridCenters[CODEC_MIP_MAX_COUNT];
		float gridScale = captureContext->desc.gridScaleMin;
		for ( u32 gridID = 0;  gridID < CODEC_MIP_MAX_COUNT; ++gridID )
		{
			gridScales[gridID] = gridScale;
			gridCenters[gridID] = Codec_ComputeGridCenter( &captureContext->frameState.origin, gridScale, gridMacroHalfWidth );
			halfCellSizes[gridID] = gridScale / gridWidth;
			if ( gridScale < captureContext->desc.gridScaleMax )
				gridScale *= 2;
		}

		const hlsl::Sample* samples = (hlsl::Sample*)GPUBuffer_MapReadBack( &res->samples );
		for ( u32 sampleID = 0; sampleID < sampleCount; ++sampleID )
		{
			Vec3u cellCoords;
			cellCoords.x = samples[sampleID].row0 >> 20;
			cellCoords.y = (samples[sampleID].row0 >> 8) & 0xFFF;
			cellCoords.z = samples[sampleID].row1 >> 20;

			const u32 mip = samples[sampleID].row1 & 0xF;

			Vec3 pMin;
			pMin.x = gridCenters[mip].x + (cellCoords.x * halfCellSizes[mip] * 2.0f ) - gridScales[mip];
			pMin.y = gridCenters[mip].y + (cellCoords.y * halfCellSizes[mip] * 2.0f ) - gridScales[mip];
			pMin.z = gridCenters[mip].z + (cellCoords.z * halfCellSizes[mip] * 2.0f ) - gridScales[mip];
			const Vec3 pMax = pMin + halfCellSizes[mip] * 2.0f;
			Plot_AddBox( &s_plot, &pMin, &pMax, false );
			Plot_AddBox( &s_plot, &pMin, &pMax, true );
		}
		GPUBuffer_UnmapReadBack( &res->samples );
	}
#endif // #if CAPTURE_DEBUG == 1
}

static u32 BuildNode( CaptureContext_s* captureContext )
{
	GPUEvent_Begin( "BuildNode");

	GPUCaptureResources_s* res = captureContext->res;

	// Set
	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &res->cbOctree.buf );
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &res->samples.srv );
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &res->sampleIndirectArgs.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &res->sampleNodeOffsets.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &res->firstChildOffsets.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &res->leaves.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &res->octreeIndirectArgs.uav, nullptr );
	g_deviceContext->CSSetShader( res->computeBuildInner.m_computeShader, nullptr, 0 );

	const u32 levelCount = captureContext->desc.gridMacroShift + 2;
	for ( u32 level = 0; level < levelCount; ++level )
	{
		// Update buffers
		{
			v6::hlsl::CBOctree* cbOctree = (v6::hlsl::CBOctree*)GPUConstantBuffer_MapWrite( &res->cbOctree );
			cbOctree->c_octreeCurrentLevel = level;
			cbOctree->c_octreeLevelCount = levelCount;
			cbOctree->c_octreeCurrentBucket = 0;
			GPUConstantBuffer_UnmapWrite( &res->cbOctree );
		}

		if ( level == levelCount-1 )
			g_deviceContext->CSSetShader( res->computeBuildLeaf.m_computeShader, nullptr, 0 );

		// Dispatch
		g_deviceContext->DispatchIndirect( res->sampleIndirectArgs.buf, sample_groupCountX_offset * sizeof( u32 ) );
	}

	// Unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	GPUEvent_End();

	const u32* octreeIndirectArgs = (u32*)GPUBuffer_MapReadBack( &res->octreeIndirectArgs );

	if ( captureContext->desc.logReadBack )
	{
		V6_MSG( "\n" );
		ReadBack_Log( "octreeContext", octreeIndirectArgs[octree_nodeCount_offset], "nodeCount" );
		V6_ASSERT( octreeIndirectArgs[octree_nodeCount_offset] <= captureContext->resNodeCount );
		ReadBack_Log( "octreeContext", octreeIndirectArgs[octree_leafGroupCountX_offset], "leafGroupCountX" );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountY_offset] == 1 );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountZ_offset] == 1 );
		ReadBack_Log( "octreeContext", octreeIndirectArgs[octree_leafCount_offset], "leafCount" );
	}

	const u32 addedLeafCount = octreeIndirectArgs[octree_leafCount_offset];
	V6_ASSERT( addedLeafCount <= captureContext->resLeafCount );

	GPUBuffer_UnmapReadBack( &res->octreeIndirectArgs );

	return addedLeafCount;
}

static void FillLeaf( CaptureContext_s* captureContext )
{
	GPUEvent_Begin( "FillLeaf");

	GPUCaptureResources_s* res = captureContext->res;

	// Update buffers
	{
		v6::hlsl::CBOctree* cbOctree = (v6::hlsl::CBOctree*)GPUConstantBuffer_MapWrite( &res->cbOctree );
		cbOctree->c_octreeCurrentLevel = 0;
		cbOctree->c_octreeLevelCount = 0;
		cbOctree->c_octreeCurrentBucket = 0;
		GPUConstantBuffer_UnmapWrite( &res->cbOctree );
	}

	// Set
	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &res->cbOctree.buf );
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &res->samples.srv );
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &res->sampleIndirectArgs.srv );
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &res->sampleNodeOffsets.srv );
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &res->firstChildOffsets.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &res->leaves.uav, nullptr );
	g_deviceContext->CSSetShader( res->computeFillLeaf.m_computeShader, nullptr, 0 );

	// Dispatch
	g_deviceContext->DispatchIndirect( res->sampleIndirectArgs.buf, sample_groupCountX_offset * sizeof( u32 ) );

	// Unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();
}

static void PackColor( CaptureContext_s* captureContext )
{
	GPUEvent_Begin( "Pack");

	GPUCaptureResources_s* res = captureContext->res;

	// Clear
	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( res->blockIndirectArgs.uav, values );

	// Set
	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &res->cbOctree.buf );
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &res->firstChildOffsets.srv );
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, &res->leaves.srv );
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &res->octreeIndirectArgs.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_POS_SLOT, 1, &res->blockPos.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_DATA_SLOT, 1, &res->blockData.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &res->blockIndirectArgs.uav, nullptr );
	g_deviceContext->CSSetShader( res->computePackColor.m_computeShader, nullptr, 0 );

	for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		// Update buffers
		{
			v6::hlsl::CBOctree* cbOctree = (v6::hlsl::CBOctree*)GPUConstantBuffer_MapWrite( &res->cbOctree );
			cbOctree->c_octreeCurrentLevel = 0;
			cbOctree->c_octreeLevelCount = captureContext->desc.gridMacroShift + 2;
			cbOctree->c_octreeCurrentBucket = bucket;
			GPUConstantBuffer_UnmapWrite( &res->cbOctree );
		}

		// Dispatch
		g_deviceContext->DispatchIndirect( res->octreeIndirectArgs.buf, octree_leafGroupCountX_offset * sizeof( u32 ) );
	}

	// Unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_POS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();

	const u32* blockIndirectArgs = (u32*)GPUBuffer_MapReadBack( &res->blockIndirectArgs );

	u32 allBlockCount = 0;
	u32 allRealCellCount = 0;
	u32 allMaxCellCount = 0; 

	for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		if ( block_count( bucket ) == 0 )
			continue;

		static const u32 cellPerBucketCounts[] = { 4, 8, 16, 32, 64 };
		const u32 maxCellCount = block_count( bucket ) * cellPerBucketCounts[bucket];

		if ( captureContext->desc.logReadBack )
		{
			V6_MSG( "\n" );
			ReadBack_Log( "blockContext", bucket, "bucket" );
			ReadBack_Log( "blockContext", block_groupCountX( bucket ), "groupCountX" );
			V6_ASSERT( block_groupCountY( bucket ) == 1 );
			V6_ASSERT( block_groupCountZ( bucket ) == 1 );
			ReadBack_Log( "blockContext", block_count( bucket ), "blockCount" );
			ReadBack_Log( "blockContext", block_posOffset( bucket ), "posOffset" );
			ReadBack_Log( "blockContext", block_dataOffset( bucket ), "dataOffset" );
			ReadBack_Log( "blockContext", block_cellCount( bucket ), "realCellCount" );
			ReadBack_Log( "blockContext", maxCellCount, "maxCellCount" );
#if HLSL_DEBUG_OCCUPANCY == 1
			ReadBack_Log( "blockContext", block_uniqueOccupancyCount( bucket ) / (float)block_count( bucket ), "avgOccupancyCount" );
			ReadBack_Log( "blockContext", block_uniqueOccupancyMax( bucket ), "maxOccupancyCount" );
			ReadBack_Log( "blockContext", block_slotOccupancyCount( bucket ) / (float)block_cellCount( bucket ), "avgOccupancySlot" );
#endif // #if HLSL_DEBUG_OCCUPANCY == 1
		}

		allBlockCount += block_count( bucket );
		allRealCellCount += block_cellCount( bucket );
		allMaxCellCount += maxCellCount;
	}		

#if 0
	V6_MSG( "\n" );
	ReadBack_Log( "packed", allBlockCount, "blockCount" );
	ReadBack_Log( "packed", allRealCellCount, "realCellCount" );
	ReadBack_Log( "packed", allMaxCellCount, "maxCellCount" );
#endif
	V6_ASSERT( allBlockCount <= captureContext->resBlockPosCount );
	V6_ASSERT( allMaxCellCount <= captureContext->resBlockDataCount );

	GPUBuffer_UnmapReadBack( &res->blockIndirectArgs );
}

void CaptureContext_Create( CaptureContext_s* captureContext, const CaptureDesc_s* desc )
{
	V6_ASSERT( s_gpuCaptureResourcesCreated == false );
	V6_ASSERT( desc->sampleCount > 0 && desc->sampleCount <= CaptureContext_s::SAMPLE_MAX_COUNT );
	GPUCaptureResources_s* res = &s_gpuCaptureResources;

	const u32 gridWidth = 1 << (desc->gridMacroShift + 2);
	const u32 renderTargetSize = gridWidth * HLSL_CELL_SUPER_SAMPLING_WIDTH;

	captureContext->desc = *desc;
	captureContext->res = res;
	captureContext->resSampleCount = (u32)(renderTargetSize * renderTargetSize / AVERAGE_SAMPLE_PER_PIXEL);
	captureContext->resLeafCount = (u32)(gridWidth * gridWidth * 6 * AVERAGE_LAYER_COUNT);
	captureContext->resNodeCount = captureContext->resLeafCount * 3;
	captureContext->resBlockDataCount = captureContext->resLeafCount * 33 / 16;
	captureContext->resBlockPosCount = captureContext->resBlockDataCount / 8;
	
	const float cellScaleMin = desc->gridScaleMin / gridWidth;
	const float freeScale = desc->gridScaleMin - cellScaleMin;
	captureContext->sampleOffsets[0] = Vec3_Make( 0.0f, 0.0f, 0.0f );
	for ( u32 sample = 1; sample < desc->sampleCount; ++sample )
	{
		if ( sample <= 8 )
		{
			u32 vertexID = sample-1;
			captureContext->sampleOffsets[sample].x = (vertexID&1) == 0 ? -freeScale : freeScale;
			captureContext->sampleOffsets[sample].y = (vertexID&2) == 0 ? -freeScale : freeScale;
			captureContext->sampleOffsets[sample].z = (vertexID&4) == 0 ? -freeScale : freeScale;
		}
		else
		{
			captureContext->sampleOffsets[sample] = Vec3_Rand() * freeScale;
		}
	}

	GPUConstantBuffer_Create( &res->cbCollect, sizeof( hlsl::CBSample ), "sample" );
	GPUConstantBuffer_Create( &res->cbOctree, sizeof( hlsl::CBOctree ), "octree" );
	
#if CAPTURE_DEBUG == 1
	Plot_Create( &s_plot, "d:/tmp/plot/capture" );
	GPUBuffer_CreateStructured( &res->samples, sizeof( hlsl::Sample ), captureContext->resSampleCount, GPUBUFFER_CREATION_FLAG_READ_BACK, "samples" );
#else
	GPUBuffer_CreateStructured( &res->samples, sizeof( hlsl::Sample ), captureContext->resSampleCount, 0, "samples" );
#endif
	GPUBuffer_CreateIndirectArgs( &res->sampleIndirectArgs, sample_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "sampleIndirectArgs" );
	GPUBuffer_CreateTyped( &res->sampleNodeOffsets, DXGI_FORMAT_R32_UINT, captureContext->resSampleCount, 0, "octreeSampleNodeOffsets" );
	GPUBuffer_CreateTyped( &res->firstChildOffsets, DXGI_FORMAT_R32_UINT, captureContext->resNodeCount, 0, "octreeFirstChildOffsets" );
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = CODEC_MIP_MAX_COUNT * 8;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( g_device->CreateUnorderedAccessView( res->firstChildOffsets.buf, &uavDesc, &res->firstChildOffsetsLimitedUAV ) );
	}
	GPUBuffer_CreateStructured( &res->leaves, sizeof( hlsl::OctreeLeaf ), captureContext->resLeafCount, 0, "octreeLeaves" );
	GPUBuffer_CreateIndirectArgs( &res->octreeIndirectArgs, octree_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "octreeIndirectArgs" );
	GPUBuffer_CreateTyped( &res->blockPos, DXGI_FORMAT_R32_UINT, captureContext->resBlockPosCount, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockPositions" );
	GPUBuffer_CreateTyped( &res->blockData, DXGI_FORMAT_R32_UINT, captureContext->resBlockDataCount, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockData" );
	GPUBuffer_CreateIndirectArgs( &res->blockIndirectArgs, block_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockIndirectArgs" );

	GPUCompute_CreateFromSource( &res->computeCollect, hlsl::g_main_sample_collect_cs, sizeof( hlsl::g_main_sample_collect_cs ) );
	GPUCompute_CreateFromSource( &res->computeBuildInner, hlsl::g_main_octree_build_inner_cs, sizeof( hlsl::g_main_octree_build_inner_cs ) );
	GPUCompute_CreateFromSource( &res->computeBuildLeaf, hlsl::g_main_octree_build_leaf_cs, sizeof( hlsl::g_main_octree_build_leaf_cs ) );
	GPUCompute_CreateFromSource( &res->computeFillLeaf, hlsl::g_main_octree_fill_leaf_cs, sizeof( hlsl::g_main_octree_fill_leaf_cs ) );
	GPUCompute_CreateFromSource( &res->computePackColor, hlsl::g_main_octree_pack_cs, sizeof( hlsl::g_main_octree_pack_cs ) );

	s_gpuCaptureResourcesCreated = true;
}

void CaptureContext_Release( CaptureContext_s* captureContext )
{
	V6_ASSERT( s_gpuCaptureResourcesCreated == true );
	GPUCaptureResources_s* res = captureContext->res;

	GPUConstantBuffer_Release( &res->cbCollect );
	GPUConstantBuffer_Release( &res->cbOctree );

	GPUBuffer_Release( &res->samples );
	GPUBuffer_Release( &res->sampleIndirectArgs );
	GPUBuffer_Release( &res->sampleNodeOffsets );
	GPUBuffer_Release( &res->firstChildOffsets );
	V6_RELEASE_D3D11( res->firstChildOffsetsLimitedUAV );
	GPUBuffer_Release( &res->leaves );
	GPUBuffer_Release( &res->octreeIndirectArgs );
	GPUBuffer_Release( &res->blockPos );
	GPUBuffer_Release( &res->blockData );
	GPUBuffer_Release( &res->blockIndirectArgs );

	GPUCompute_Release( &res->computeCollect );
	GPUCompute_Release( &res->computeBuildInner );
	GPUCompute_Release( &res->computeBuildLeaf );
	GPUCompute_Release( &res->computeFillLeaf );
	GPUCompute_Release( &res->computePackColor );

#if CAPTURE_DEBUG == 1
	Plot_Release( &s_plot );
#endif // #if CAPTURE_DEBUG == 1

	s_gpuCaptureResourcesCreated = false;
}

void CaptureContext_Begin( CaptureContext_s* captureContext, const Vec3* origin )
{
	ClearNode( captureContext );
	captureContext->frameState.origin = *origin;
}

void CaptureContext_End( CaptureContext_s* captureContext )
{
	PackColor( captureContext );
}

Vec3 CaptureContext_ComputeSamplePos( CaptureContext_s* captureContext, const Vec3* origin, u32 sampleID )
{
	const u32 gridMacroHalfWidth = 1 << (captureContext->desc.gridMacroShift-1);
	const Vec3 gridCenter = Codec_ComputeGridCenter( origin, captureContext->desc.gridScaleMin, gridMacroHalfWidth );
	return gridCenter + captureContext->sampleOffsets[sampleID];
}

u32 CaptureContext_AddSamplesFromCubeFace( CaptureContext_s* captureContext, const Vec3* samplePos, const Vec3 basis[3], void* colorView, void* depthView )
{
	Collect( captureContext, samplePos, basis, (ID3D11ShaderResourceView*)colorView, (ID3D11ShaderResourceView*)depthView );
	const u32 sumLeafCount = BuildNode( captureContext );
	FillLeaf( captureContext );
	return sumLeafCount;
}

void CaptureContext_MapBlocksForRead( CaptureContext_s* captureContext, u32* blockCounts, void** blockPos, void** blockData )
{
	GPUCaptureResources_s* res = captureContext->res;

	{
		const u32* blockIndirectArgs = (u32*)GPUBuffer_MapReadBack( &res->blockIndirectArgs );

		for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
			blockCounts[bucket] = block_count( bucket );

		GPUBuffer_UnmapReadBack( &res->blockIndirectArgs );
	}

	*blockPos = (void*)GPUBuffer_MapReadBack( &res->blockPos );
	*blockData = (void*)GPUBuffer_MapReadBack( &res->blockData );
}

void CaptureContext_UnmapBlocksForRead( CaptureContext_s* captureContext )
{
	GPUCaptureResources_s* res = captureContext->res;

	GPUBuffer_UnmapReadBack( &res->blockPos );
	GPUBuffer_UnmapReadBack( &res->blockData );
}

END_V6_NAMESPACE
