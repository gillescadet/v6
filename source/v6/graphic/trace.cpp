/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <d3d11_1.h>
#include <v6/core/windows_end.h>

#include <v6/core/memory.h>
#include <v6/core/plot.h>
#include <v6/core/random.h>
#include <v6/core/string.h>
#include <v6/codec/decoder.h>
#include <v6/graphic/trace.h>
#include <v6/graphic/trace_shaders.h>
#include <v6/graphic/trace_shared.h>
#include <v6/graphic/view.h>

#define TRACE_TSAA_SAMPLE_COUNT		64
#define TRACE_TSAA_BLEND_FACTOR		0.15f

#define TRACE_CELL_DEBUG			0

BEGIN_V6_NAMESPACE

struct GPUTraceResources_s
{
	GPUConstantBuffer_s		cbCull;
	GPUConstantBuffer_s		cbTrace;
	GPUConstantBuffer_s		cbBlend;
	GPUConstantBuffer_s		cbTSAA;

	GPUBuffer_s				blockPos;
	GPUBuffer_s				blockData;
	GPUBuffer_s				ranges[2];
	GPUBuffer_s				groups[2];

	GPUBuffer_s				traceCell;
	GPUBuffer_s				traceIndirectArgs;
	
	GPUBuffer_s				cellItems;
	GPUBuffer_s				cellItemCounters;

	GPUTexture2D_s			colors;
	GPUTexture2D_s			histories[2][2];
	GPUBuffer_s				displacements;

	ID3D11SamplerState*		bilinearSamplerState;

	GPUBuffer_s				cullStats;
	GPUBuffer_s				traceStats;
	GPUBuffer_s				traceCellStats;
	GPUBuffer_s				blendStats;

	GPUCompute_s			computeCull[2][CODEC_BUCKET_COUNT];
	GPUCompute_s			computeTraceInit;
	GPUCompute_s			computeTrace[2][CODEC_BUCKET_COUNT];
	GPUCompute_s			computeBlend[2];
	GPUCompute_s			computeTSAA;
	GPUCompute_s			computeSharpen;
};

static const GPUEventID_t s_gpuEventCull			= GPUEvent_Register( "Cull", true );
static const GPUEventID_t s_gpuEventCullBucket		= GPUEvent_Register( "Cull Bucket", false );
static const GPUEventID_t s_gpuEventTrace			= GPUEvent_Register( "Trace", true );
static const GPUEventID_t s_gpuEventInitTrace		= GPUEvent_Register( "Init Trace", false );
static const GPUEventID_t s_gpuEventTraceBucket		= GPUEvent_Register( "Trace Bucket", false );
static const GPUEventID_t s_gpuEventBlends[2]		= { GPUEvent_Register( "Blend_L", true ), GPUEvent_Register( "Blend_R", true ) };
static const GPUEventID_t s_gpuEventTSAAs[2]		= { GPUEvent_Register( "TSAA_L", true ), GPUEvent_Register( "TSAA_R", true ) };
static const GPUEventID_t s_gpuEventSharpens[2]		= { GPUEvent_Register( "Sharpen_L", true ), GPUEvent_Register( "Sharpen_R", true ) };

extern ID3D11Device*							g_device;
extern ID3D11DeviceContext*						g_deviceContext;

static GPUTraceResources_s						s_gpuTraceResources;
static bool										s_gpuTraceResourcesCreated = false;

static const u32 SEQUENCE_BLOCK_GROUP_MAX_COUNT = 65536;

static void CullBlock( TraceContext_s* traceContext, const View_s* views, const TraceOptions_s* options )
{
	static const void* nulls[8] = {};

	GPUTraceResources_s* traceRes = traceContext->res;

	GPUEvent_Begin( s_gpuEventCull );

	// Clear

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( traceRes->traceIndirectArgs.uav, values );
	if ( options->logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->cullStats.uav, values );

	// Update buffers

	V6_ASSERT( traceContext->stream->desc.gridMacroShift > 0 );
	const u32 gridMacroHalfWidth = 1 << (traceContext->stream->desc.gridMacroShift-1);
	const u32 gridWidth = 1 << (traceContext->stream->desc.gridMacroShift + 2);
	const float invGridWidth = 1.0f / gridWidth;
	const u32 leftEye = 0;
	const u32 rightEye = traceContext->desc.stereo ? 1 : 0;

	v6::hlsl::CBCull cbCullData = {};
	{
		cbCullData.c_cullGridMacroShift = traceContext->stream->desc.gridMacroShift;
		cbCullData.c_cullInvGridWidth = invGridWidth;

		float gridScale = traceContext->stream->desc.gridScaleMin;
		for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
		{
			const Vec3 center = Codec_ComputeGridCenter( &traceContext->frameState.origin, gridScale, gridMacroHalfWidth );
			cbCullData.c_cullCentersAndGridScales[gridID] = Vec4_Make( &center, gridScale );
			if ( gridScale < traceContext->stream->desc.gridScaleMax )
				gridScale *= 2;
		}

		V6_ASSERT( views[leftEye].forward == views[rightEye].forward );
		V6_ASSERT( views[leftEye].right == views[rightEye].right );
		V6_ASSERT( views[leftEye].up == views[rightEye].up );

		const float tanHalfFOVLeft = Max( views[leftEye].tanHalfFOVLeft, views[rightEye].tanHalfFOVLeft );
		const float tanHalfFOVRight = Max( views[leftEye].tanHalfFOVRight, views[rightEye].tanHalfFOVRight );
		const float tanHalfFOVUp = Max( views[leftEye].tanHalfFOVUp, views[rightEye].tanHalfFOVUp );
		const float tanHalfFOVDown = Max( views[leftEye].tanHalfFOVDown, views[rightEye].tanHalfFOVDown );

		const Vec3 leftPlane = (views[0].forward * tanHalfFOVLeft + views[0].right).Normalized();
		const Vec3 rightPlane = (views[0].forward * tanHalfFOVRight - views[0].right).Normalized();
		const Vec3 upPlane = (views[0].forward * tanHalfFOVUp - views[0].up).Normalized();
		const Vec3 bottomPlane = (views[0].forward * tanHalfFOVDown + views[0].up).Normalized();
		
		cbCullData.c_cullFrustumPlanes[0] = Vec4_Make( &leftPlane, -Dot( leftPlane, views[leftEye].org ) );
		cbCullData.c_cullFrustumPlanes[1] = Vec4_Make( &rightPlane, -Dot( rightPlane, views[rightEye].org ) );
		cbCullData.c_cullFrustumPlanes[2] = Vec4_Make( &upPlane, -Dot( upPlane, views[0].org ) );
		cbCullData.c_cullFrustumPlanes[3] = Vec4_Make( &bottomPlane, -Dot( bottomPlane, views[0].org ) );
	}

	// set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBCullSlot, 1, &traceRes->cbCull.buf );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, &traceRes->groups[traceContext->frameState.bufferID].srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, &traceRes->ranges[traceContext->frameState.bufferID].srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &traceRes->blockPos.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, &traceRes->traceCell.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &traceRes->traceIndirectArgs.uav, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, &traceRes->cullStats.uav, nullptr );

	for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		GPUEvent_Begin( s_gpuEventCullBucket );

		// update
		{
			cbCullData.c_cullBlockGroupCount = traceContext->frameState.groupCounts[bucket];

			v6::hlsl::CBCull* cbCull = (v6::hlsl::CBCull*)GPUConstantBuffer_MapWrite( &traceRes->cbCull );
			memcpy( cbCull, &cbCullData, sizeof( cbCullData ) );
			GPUConstantBuffer_UnmapWrite( &traceRes->cbCull );
			
			cbCullData.c_cullBlockGroupOffset += traceContext->frameState.groupCounts[bucket];
			cbCullData.c_cullBlockRangeOffset += traceContext->frameState.blockRangeCounts[bucket];
		}

		// dispach
		const u32 shaderOption = (options->logReadBack || options->showHistory) ? 1 : 0;
		GPUCompute_Dispatch( &traceRes->computeCull[shaderOption][bucket], Max( 1u, traceContext->frameState.groupCounts[bucket] ), 1, 1 );

		GPUEvent_End();
	}
	
	// Unset
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();

	if ( options->logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlockCullStats* blockCullStats = (hlsl::BlockCullStats*)GPUBuffer_MapReadBack( &traceRes->cullStats );

			ReadBack_Log( "blockCull", blockCullStats->blockInputCount, "blockInputCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockProcessedCount, "blockProcessedCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockPassedCount, "blockPassedCount" );
			V6_ASSERT( blockCullStats->blockPassedCount <= traceContext->resPassedBlockCount );
			u32 cellOutputCount = 0;
			for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
			{
				if ( blockCullStats->cellOutputCounts[mip] )
				{
					ReadBack_Log( "blockCull", blockCullStats->cellOutputCounts[mip], String_Format( "cellOutputCount.mip[%d]", mip ) );
					cellOutputCount += blockCullStats->cellOutputCounts[mip];
				}
			}
			ReadBack_Log( "blockCull", cellOutputCount, "cellOutputCount" );

			GPUBuffer_UnmapReadBack( &traceRes->cullStats );
		}
	}
}

static int BlockTraceCellStats_Compare( const void* blockTraceCellStatsPointer0, const void* blockTraceCellStatsPointer1 )
{
	const hlsl::BlockTraceCellStats* blockTraceCellStats0 = (hlsl::BlockTraceCellStats*)blockTraceCellStatsPointer0;
	const hlsl::BlockTraceCellStats* blockTraceCellStats1 = (hlsl::BlockTraceCellStats*)blockTraceCellStatsPointer1;
	return blockTraceCellStats0->blockCellID - blockTraceCellStats1->blockCellID;
}

static void TraceBlock( TraceContext_s* traceContext, const View_s* views, const TraceOptions_s* options, IStack* stack )
{
	static const void* nulls[8] = {};
	
	GPUTraceResources_s* traceRes = traceContext->res;

	GPUEvent_Begin( s_gpuEventTrace );

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( traceRes->cellItemCounters.uav, values );
	if ( options->logReadBack )
	{
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->traceStats.uav, values );
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->traceCellStats.uav, values );
	}

	{
		V6_ASSERT( traceContext->stream->desc.gridMacroShift > 0 );
		const u32 gridMacroHalfWidth = 1 << (traceContext->stream->desc.gridMacroShift-1);
		const u32 gridWidth = 1 << (traceContext->stream->desc.gridMacroShift + 2);
		const float invGridWidth = 1.0f / gridWidth;
		
		const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;

		Mat4x4 worldToProjs[2];
		for ( u32 eye = 0; eye < eyeCount; ++eye )
			Mat4x4_Mul( &worldToProjs[eye], views[eye].projMatrix, views[eye].viewMatrix );

		if ( traceContext->frameState.resetJitter )
		{
			for ( u32 eye = 0; eye < eyeCount; ++eye )
			{
				traceContext->frameState.prevWorldToProjsX[eye] = worldToProjs[eye].m_row0;
				traceContext->frameState.prevWorldToProjsY[eye] = worldToProjs[eye].m_row1;
				traceContext->frameState.prevWorldToProjsW[eye] = worldToProjs[eye].m_row3;
			}
			traceContext->frameState.jitterID = 0;
		}

		traceContext->frameState.curHistoryBufferID = traceContext->frameState.jitterID & 1;
		traceContext->frameState.prevHistoryBufferID = traceContext->frameState.curHistoryBufferID ^ 1;

		if ( options->noTSAA )
		{
			traceContext->frameState.jitter = Vec2_Make( 0.5f, 0.5f );
		}
		else
		{
			traceContext->frameState.jitter = Vec2_Make( HaltonSequence< 2 >( traceContext->frameState.jitterID ), HaltonSequence< 3 >( traceContext->frameState.jitterID ) );
			traceContext->frameState.jitterID = (traceContext->frameState.jitterID + 1) % TRACE_TSAA_SAMPLE_COUNT;
		}

		v6::hlsl::CBTrace* cbTrace = (v6::hlsl::CBTrace*)GPUConstantBuffer_MapWrite( &traceRes->cbTrace );

		float gridScale = traceContext->stream->desc.gridScaleMin;
		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip )
		{
			const float halfCellSize = gridScale * invGridWidth;
			cbTrace->c_traceGridScales[mip] = Vec4_Make( gridScale, halfCellSize, 0.0f, 0.0f );
			const Vec3 center = Codec_ComputeGridCenter( &traceContext->frameState.origin, gridScale, gridMacroHalfWidth );
			cbTrace->c_traceGridCenters[mip] = Vec4_Make( &center, 0.0f );
			if ( gridScale < traceContext->stream->desc.gridScaleMax )
				gridScale *= 2;
		}

		cbTrace->c_traceGridMacroShift = traceContext->stream->desc.gridMacroShift;
		cbTrace->c_traceEyeCount = eyeCount;

		const Vec2 frameSize = Vec2_Make( (float)traceContext->desc.screenWidth, (float)traceContext->desc.screenHeight );
		cbTrace->c_traceFrameSize = frameSize;

		for ( u32 eye = 0; eye < eyeCount; ++eye )
		{	
			hlsl::TracePerEye tracePerEye;
			tracePerEye.prevWorldToProjX = traceContext->frameState.prevWorldToProjsX[eye];
			tracePerEye.prevWorldToProjY = traceContext->frameState.prevWorldToProjsY[eye];
			tracePerEye.prevWorldToProjW = traceContext->frameState.prevWorldToProjsW[eye];
			tracePerEye.curWorldToProjX = worldToProjs[eye].m_row0;
			tracePerEye.curWorldToProjY = worldToProjs[eye].m_row1;
			tracePerEye.curWorldToProjW = worldToProjs[eye].m_row3;

			const float scaleRight = (views[eye].tanHalfFOVLeft + views[eye].tanHalfFOVRight) / frameSize.x;
			const float scaleUp = (views[eye].tanHalfFOVUp + views[eye].tanHalfFOVDown) / frameSize.y;

			const Vec3 forward = views[eye].forward;
			const Vec3 right = views[eye].right * scaleRight;
			const Vec3 up = views[eye].up * scaleUp;
				
			tracePerEye.org = views[eye].org;

			tracePerEye.rayDirBase = forward - views[eye].up * views[eye].tanHalfFOVDown - views[eye].right * views[eye].tanHalfFOVLeft;
			tracePerEye.rayDirUp = up;
			tracePerEye.rayDirRight = right;

			cbTrace->c_traceEyes[eye] = tracePerEye;

			cbTrace->c_traceGetStats = options->logReadBack;
			cbTrace->c_traceShowFlag = (options->showMip ? HLSL_BLOCK_SHOW_FLAG_MIPS : 0) | (options->showBucket ? HLSL_BLOCK_SHOW_FLAG_BUCKETS : 0);
		}

		cbTrace->c_traceJitter = traceContext->frameState.jitter;

		GPUConstantBuffer_UnmapWrite( &traceRes->cbTrace );

		for ( u32 eye = 0; eye < eyeCount; ++eye )
		{
			traceContext->frameState.prevWorldToProjsX[eye] = worldToProjs[eye].m_row0;
			traceContext->frameState.prevWorldToProjsY[eye] = worldToProjs[eye].m_row1;
			traceContext->frameState.prevWorldToProjsW[eye] = worldToProjs[eye].m_row3;
		}
	}

	// set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBTraceSlot, 1, &traceRes->cbTrace.buf );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, &traceRes->blockData.srv );
	g_deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, &traceRes->traceCell.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &traceRes->cellItems.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &traceRes->cellItemCounters.uav, nullptr );
	if ( options->logReadBack )
	{
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, &traceRes->traceStats.uav, nullptr );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELL_STATS_SLOT, 1, &traceRes->traceCellStats.uav, nullptr );
	}
	
	{
		GPUEvent_Begin( s_gpuEventInitTrace );

		// set
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &traceRes->traceIndirectArgs.uav, nullptr );

		// dispatch
		GPUCompute_Dispatch( &traceRes->computeTraceInit, 1, 1, 1 );

		// unset
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		GPUEvent_End();
	}

	for ( u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		GPUEvent_Begin( s_gpuEventTraceBucket );

		// set
		g_deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &traceRes->traceIndirectArgs.srv );

		// dispach
		const u32 shaderOption = ( options->logReadBack || options->showMip || options->showBucket || options->showHistory ) ? 1 : 0;
		GPUCompute_DispatchIndirect( &traceRes->computeTrace[shaderOption][bucket], &traceRes->traceIndirectArgs, trace_cellGroupCountX_offset( bucket ) * sizeof( u32 ) );

		// unset
		g_deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

		GPUEvent_End();
	}

	// Unset
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( options->logReadBack )
	{
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELL_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	}
	
	GPUEvent_End();

	if ( options->logReadBack )
	{			
		V6_MSG( "\n" );

		u32 traceCellStatsCount = 0;

		{
			const hlsl::BlockTraceStats* blockTraceStats = (hlsl::BlockTraceStats*)GPUBuffer_MapReadBack( &traceRes->traceStats );

			ReadBack_Log( "blockTrace", blockTraceStats->cellInputCount, "cellInputCount" );
			u32 cellProcessedCount = 0;
			for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip  )
			{
				if ( blockTraceStats->cellProcessedCounts[mip] )
				{
					ReadBack_Log( "blockTrace", blockTraceStats->cellProcessedCounts[mip], String_Format( "cellProcessedCounts.mip[%d]", mip ) );
					cellProcessedCount += blockTraceStats->cellProcessedCounts[mip];
				}
			}
			ReadBack_Log( "blockTrace", cellProcessedCount, "cellProcessedCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellValidCount, "cellValidCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelSampleCount, "pixelSampleCount" );
			for ( u32 pixelSampleCount = 0; pixelSampleCount <= 9; ++pixelSampleCount )
				ReadBack_Log( "blockTrace", blockTraceStats->pixelSampleDistribution[pixelSampleCount], String_Format( "%d_pixelSampleCount", pixelSampleCount ) );
			u32 cellItemCount = 0;
			for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip  )
			{
				if ( blockTraceStats->cellItemCounts[mip] )
				{
					ReadBack_Log( "blockTrace", blockTraceStats->cellItemCounts[mip], String_Format( "cellItemCount.mip[%d]", mip ) );
					cellItemCount += blockTraceStats->cellItemCounts[mip];
				}
			}
			ReadBack_Log( "blockTrace", cellItemCount, "cellItemCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemMaxCountPerPixel, "cellItemMaxCountPerPixel" );
			V6_ASSERT( cellItemCount < traceContext->resCellItemCount );

			ReadBack_Log( "blockTrace", blockTraceStats->traceCellStatCount, "traceCellStatCount" );
			traceCellStatsCount = blockTraceStats->traceCellStatCount;

			GPUBuffer_UnmapReadBack( &traceRes->traceStats );
		}

#if TRACE_CELL_DEBUG == 1
		if ( traceCellStatsCount )
		{
			ScopedStack scopedStack( stack );

			Plot_s plot;
			Plot_Create( &plot, "d:/tmp/plot/traceCellStats" );

			hlsl::BlockTraceCellStats* blockTraceCellStats = stack->newArray< hlsl::BlockTraceCellStats >( traceCellStatsCount );
			memcpy( blockTraceCellStats, GPUBuffer_MapReadBack( &traceRes->traceCellStats ), sizeof( *blockTraceCellStats ) * traceCellStatsCount );
			GPUBuffer_UnmapReadBack( &traceRes->traceCellStats );

			qsort( blockTraceCellStats, traceCellStatsCount, sizeof( *blockTraceCellStats ), BlockTraceCellStats_Compare );

			u32 prevBlockCellID = (u32)-1;
			bool addHitBox = false;

			const Vec3 origin = Vec3_Zero();

			for ( u32 blockTraceCellStatsID = 0; blockTraceCellStatsID < traceCellStatsCount; ++blockTraceCellStatsID )
			{
				const hlsl::BlockTraceCellStats* blockTraceCellStat = &blockTraceCellStats[blockTraceCellStatsID];

				if ( blockTraceCellStat->blockCellID != prevBlockCellID )
				{
					Plot_NewObject( &plot, Color_Make( 255, 255, 255, 255 ) );
					Plot_AddBox( &plot, &blockTraceCellStat->boxMinRS, &blockTraceCellStat->boxMaxRS, true );
					addHitBox = true;
					prevBlockCellID = blockTraceCellStat->blockCellID;
				}
				
				const Vec3 center = (blockTraceCellStat->boxMinRS + blockTraceCellStat->boxMaxRS) * 0.5f;
				const float centerDistance = center.Length() / blockTraceCellStat->rayDir.Length();

				if ( blockTraceCellStat->hit )
				{
					const Vec3 startPoint = centerDistance * 0.8f * blockTraceCellStat->rayDir;
					const Vec3 hitPoint = blockTraceCellStat->tIn * blockTraceCellStat->rayDir;
					Plot_AddLine( &plot, &startPoint, &hitPoint );
					if ( addHitBox )
					{
						Plot_AddBox( &plot, &blockTraceCellStat->boxMinRS, &blockTraceCellStat->boxMaxRS, false );
						addHitBox = false;
					}
				}
				else
				{
					const Vec3 startPoint = centerDistance * 0.8f * blockTraceCellStat->rayDir;
					const Vec3 endPoint = centerDistance * 1.2f * blockTraceCellStat->rayDir;
					Plot_AddLine( &plot, &startPoint, &endPoint );
				}
			}

			Plot_Release( &plot );
		}
#endif // #if TRACE_CELL_DEBUG == 1
	}
}

static void BlendPixel( TraceContext_s* traceContext, ID3D11UnorderedAccessView* outputColors, u32 eye, const TraceOptions_s* options )
{
	GPUTraceResources_s* traceRes = traceContext->res;

	GPUEvent_Begin( s_gpuEventBlends[eye] );

	// Clear

	u32 values[4] = {};
	if ( options->logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->blendStats.uav, values );
	
	// Set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBBlendSlot, 1, &traceRes->cbBlend.buf );

	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &traceRes->cellItems.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &traceRes->cellItemCounters.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, &outputColors, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_DISPLACEMENT_SLOT, 1, &traceRes->displacements.uav, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLEND_STATS_SLOT, 1, &traceRes->blendStats.uav, nullptr );

	const u32 shaderOption = (options->logReadBack || options->showOverdraw) ? 1 : 0;
	g_deviceContext->CSSetShader( traceRes->computeBlend[shaderOption].m_computeShader, nullptr, 0 );
	
	{
		v6::hlsl::CBBlend* cbBlend = (v6::hlsl::CBBlend*)GPUConstantBuffer_MapWrite( &traceRes->cbBlend );

		cbBlend->c_blendFrameSize.x = traceContext->desc.screenWidth;
		cbBlend->c_blendFrameSize.y = traceContext->desc.screenHeight;

		cbBlend->c_blendFrameInvSize.x = 1.0f / traceContext->desc.screenWidth;
		cbBlend->c_blendFrameInvSize.y = 1.0f / traceContext->desc.screenHeight;

		const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;
		
		cbBlend->c_blendEye = eye;
		cbBlend->c_blendEyeCount = traceContext->desc.stereo ? 2 : 1;
		
		cbBlend->c_blendDepth24Norm = ((1<<24) - 1) / traceContext->stream->desc.gridScaleMax;
		
		cbBlend->c_blendGetStats = options->logReadBack;
		cbBlend->c_blendShowOverdraw = options->showOverdraw;

		GPUConstantBuffer_UnmapWrite( &traceRes->cbBlend  );
	}

	V6_ASSERT( (traceContext->desc.screenWidth & 0x7) == 0 );
	V6_ASSERT( (traceContext->desc.screenHeight & 0x7) == 0 );
	const u32 pixelGroupWidth = traceContext->desc.screenWidth >> 3;
	const u32 pixelGroupHeight = traceContext->desc.screenHeight >> 3;
	g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_DISPLACEMENT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLEND_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();

	if ( options->logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlendStats* blendStats = (hlsl::BlendStats*)GPUBuffer_MapReadBack( &traceRes->blendStats );

			ReadBack_Log( "pixelBlend", blendStats->pixelInputCount, "pixelInputCount" );
			ReadBack_Log( "pixelBlend", blendStats->cellItemHitInputCount, "cellItemHitInputCount" );
			ReadBack_Log( "pixelBlend", blendStats->cellItemBorderInputCount, "cellItemBorderInputCount" );
			ReadBack_Log( "pixelBlend", blendStats->cellItemHitInputCount + blendStats->cellItemBorderInputCount, "cellItemInputCount" );
			ReadBack_Log( "pixelBlend", blendStats->cellItemHitProcessedCount, "cellItemHitProcessedCount" );
			ReadBack_Log( "pixelBlend", blendStats->cellItemBorderProcessedCount, "cellItemBorderProcessedCount" );
			ReadBack_Log( "pixelBlend", blendStats->cellItemHitProcessedCount + blendStats->cellItemBorderProcessedCount, "cellItemProcessedCount" );

			GPUBuffer_UnmapReadBack( &traceRes->blendStats );
		}
	}
}

static void TSAAPixel( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, u32 eye, const TraceOptions_s* options )
{
	GPUTraceResources_s* traceRes = traceContext->res;

	{
		GPUEvent_Begin( s_gpuEventTSAAs[eye] );
	
		// Set

		g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBTSAASlot, 1, &traceRes->cbTSAA.buf );

		g_deviceContext->CSSetSamplers( HLSL_BILINEAR_SLOT, 1, &traceRes->bilinearSamplerState );

		g_deviceContext->CSSetShaderResources( HLSL_COLOR_SLOT, 1, &traceRes->colors.srv );
		g_deviceContext->CSSetShaderResources( HLSL_DISPLACEMENT_SLOT, 1, &traceRes->displacements.srv );
		g_deviceContext->CSSetShaderResources( HLSL_HISTORY_SLOT, 1, &traceRes->histories[traceContext->frameState.prevHistoryBufferID][eye].srv );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, &traceRes->histories[traceContext->frameState.curHistoryBufferID][eye].uav, nullptr );
		g_deviceContext->CSSetShader( traceRes->computeTSAA.m_computeShader, nullptr, 0 );
	
		{
			const Vec2 frameSize = Vec2_Make( (float)traceContext->desc.screenWidth, (float)traceContext->desc.screenHeight );

			v6::hlsl::CBTSAA* cbTSAA = (v6::hlsl::CBTSAA*)GPUConstantBuffer_MapWrite( &traceRes->cbTSAA );

			cbTSAA->c_tsaaJitter = Vec2_Make( traceContext->frameState.jitter.x, traceContext->frameState.jitter.y );
			cbTSAA->c_tsaaBlendFactor = traceContext->frameState.resetJitter ? 1.0f : TRACE_TSAA_BLEND_FACTOR;
			cbTSAA->c_tsaaFrameSize = frameSize;
			cbTSAA->c_tsaaInvFrameSize = 1.0f / frameSize;

			GPUConstantBuffer_UnmapWrite( &traceRes->cbTSAA );
		}

		V6_ASSERT( (traceContext->desc.screenWidth & 0x7) == 0 );
		V6_ASSERT( (traceContext->desc.screenHeight & 0x7) == 0 );
		const u32 pixelGroupWidth = traceContext->desc.screenWidth >> 3;
		const u32 pixelGroupHeight = traceContext->desc.screenHeight >> 3;
		g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

		// unset
		static const void* nulls[8] = {};
		g_deviceContext->CSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		g_deviceContext->CSSetShaderResources( HLSL_DISPLACEMENT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		g_deviceContext->CSSetShaderResources( HLSL_HISTORY_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		GPUEvent_End();
	}

	if ( options->noSharpenFilter )
	{
		g_deviceContext->CopyResource( renderTargetSet->colorBuffers[eye].tex, traceRes->histories[traceContext->frameState.curHistoryBufferID][eye].tex );
	}
	else
	{
		GPUEvent_Begin( s_gpuEventSharpens[eye] );
	
		// Set

		g_deviceContext->CSSetShaderResources( HLSL_COLOR_SLOT, 1, &traceRes->histories[traceContext->frameState.curHistoryBufferID][eye].srv );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, &renderTargetSet->colorBuffers[eye].uav, nullptr );
		g_deviceContext->CSSetShader( traceRes->computeSharpen.m_computeShader, nullptr, 0 );

		V6_ASSERT( (traceContext->desc.screenWidth & 0x7) == 0 );
		V6_ASSERT( (traceContext->desc.screenHeight & 0x7) == 0 );
		const u32 pixelGroupWidth = traceContext->desc.screenWidth >> 3;
		const u32 pixelGroupHeight = traceContext->desc.screenHeight >> 3;
		g_deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

		// unset
		static const void* nulls[8] = {};
		g_deviceContext->CSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		GPUEvent_End();
	}
}

static void SequenceContext_Update( SequenceContext_s* sequenceContext, const VideoStream_s* stream, const VideoSequence_s* sequence )
{
	memset( sequenceContext, 0, sizeof( *sequenceContext ) );

	{
		u32 rangeDefOffset = 0;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			sequenceContext->rangeDefs[bucket] = sequence->data.rangeDefs + rangeDefOffset;
			rangeDefOffset += sequence->desc.rangeDefCounts[bucket];
		}
	}

	u32 nextRangeIDs[CODEC_BUCKET_COUNT] = {};
	u32 blockPosCount = 0;
	u32 blockDataCount = 0;
	for ( u32 frameID = 0; frameID < sequence->desc.frameCount; ++frameID )
	{
		if ( sequence->frameDescArray[frameID].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		sequenceContext->frameBlockPosOffsets[frameID] = blockPosCount;
		sequenceContext->frameBlockDataOffsets[frameID] = blockDataCount;

		Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
		float gridScale = stream->desc.gridScaleMin;
		const u32 gridMacroHalfWidth = 1 << (stream->desc.gridMacroShift-1);
		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
			macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameID].gridOrigin, gridScale, gridMacroHalfWidth );
		
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; )
		{
			const u32 rangeID = nextRangeIDs[bucket];

			if ( rangeID == sequence->desc.rangeDefCounts[bucket] )
			{
				++bucket;
				continue;
			}
			
			const CodecRange_s* codecRange = &sequenceContext->rangeDefs[bucket][rangeID];
			u32 rangeFrameID = codecRange->frameRank8_mip4_blockCount20 >> 24;
			if ( frameID != rangeFrameID )
			{
				++bucket;
				continue;
			}

			const u32 blockCount = codecRange->frameRank8_mip4_blockCount20 & 0xFFFFF;
			const u32 mip = (codecRange->frameRank8_mip4_blockCount20 >> 20) & 0xF;

			SequenceBlockRange_s* blockRange = &sequenceContext->blockRanges[bucket][rangeID];
			
			blockRange->macroGridCoords = macroGridCoords[mip];
			blockRange->blockCount = blockCount;
			blockRange->blockPosOffset = blockPosCount;
			blockRange->blockDataOffset = blockDataCount;

			const u32 cellPerBucketCount = 1 << (2 + bucket);
			blockPosCount += blockCount;
			blockDataCount += blockCount * cellPerBucketCount;

			++nextRangeIDs[bucket];
		}
	}

	for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		V6_ASSERT( nextRangeIDs[bucket] == sequence->desc.rangeDefCounts[bucket] );
}

void TraceContext_Create( TraceContext_s* traceContext, const TraceDesc_s* traceDesc, const VideoStream_s* stream )
{
	V6_STATIC_ASSERT( CODEC_BLOCK_THREAD_GROUP_SIZE == HLSL_BLOCK_THREAD_GROUP_SIZE );

	memset( traceContext, 0, sizeof( *traceContext ) );
	
	V6_ASSERT( s_gpuTraceResourcesCreated == false );
	GPUTraceResources_s* res = &s_gpuTraceResources;

	V6_ASSERT( traceDesc->screenWidth > 0 );
	V6_ASSERT( traceDesc->screenHeight > 0 );

	traceContext->desc = *traceDesc;
	traceContext->stream = stream;
	traceContext->frameState.sequenceID = (u32)-1;
	traceContext->frameState.frameID =(u32)-1;

	const u32 blockPosCount = Max( 1u, stream->desc.maxBlockPosCountPerSequence );
	const u32 blockDataCount = Max( 1u, stream->desc.maxBlockDataCountPerSequence );
	const u32 maxBlockRangeCount = Max( 1u, stream->desc.maxBlockRangeCountPerFrame );
	const u32 maxBlockGroupCount = Max( 1u, stream->desc.maxBlockGroupCountPerFrame );

	V6_ASSERT( maxBlockGroupCount <= SEQUENCE_BLOCK_GROUP_MAX_COUNT );

	GPUBuffer_CreateTyped( &res->blockPos, DXGI_FORMAT_R32_UINT, blockPosCount, GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE, "sequenceBlockPositions" );
	GPUBuffer_CreateTyped( &res->blockData, DXGI_FORMAT_R32_UINT, blockDataCount, GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE, "sequenceBlockData" );
	GPUBuffer_CreateStructured( &res->ranges[0], sizeof( hlsl::BlockRange ), maxBlockRangeCount, GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE, "sequenceBlockRanges0" );
	GPUBuffer_CreateStructured( &res->ranges[1], sizeof( hlsl::BlockRange ), maxBlockRangeCount, GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE, "sequenceBlockRanges1" );
	GPUBuffer_CreateTyped( &res->groups[0], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE, "sequenceBlockGroups0" );
	GPUBuffer_CreateTyped( &res->groups[1], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE, "sequenceBlockGroups1" );
	
	const u32 eyeCount = traceDesc->stereo ? 2 : 1;
	traceContext->resPassedBlockCount = Max( 1u, stream->desc.maxBlockCountPerFrame );
	traceContext->resCellItemCount = Max( 1u, (traceDesc->screenWidth * eyeCount * traceDesc->screenHeight) * HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT );

	GPUConstantBuffer_Create( &res->cbCull, sizeof( v6::hlsl::CBCull ), "cull" );
	GPUConstantBuffer_Create( &res->cbTrace, sizeof( v6::hlsl::CBTrace ), "trace" );
	GPUConstantBuffer_Create( &res->cbBlend, sizeof( v6::hlsl::CBBlend), "blend" );
	GPUConstantBuffer_Create( &res->cbTSAA, sizeof( v6::hlsl::CBTSAA), "tsaa" );

	GPUBuffer_CreateTyped( &res->traceCell, DXGI_FORMAT_R32_UINT, traceContext->resPassedBlockCount * 2, 0, "traceCell" );
	GPUBuffer_CreateIndirectArgs( &res->traceIndirectArgs, trace_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "traceIndirectArgs" );

	GPUBuffer_CreateStructured( &res->cellItems, sizeof( hlsl::BlockCellItem ), traceContext->resCellItemCount, 0, "blockCellItems" );
	GPUBuffer_CreateTyped( &res->cellItemCounters, DXGI_FORMAT_R32_UINT, traceDesc->screenWidth * eyeCount * traceDesc->screenHeight, 0, "blockCellItemCounters" );	
	
	GPUBuffer_CreateStructured( &res->cullStats, sizeof( hlsl::BlockCullStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockCullStats" );
	GPUBuffer_CreateStructured( &res->traceStats, sizeof( hlsl::BlockTraceStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockTraceStats" );
	GPUBuffer_CreateStructured( &res->traceCellStats, sizeof( hlsl::BlockTraceCellStats ), HLSL_BLOCK_TRACE_CELL_STATS_MAX_COUNT, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockTraceCellStats" );
	GPUBuffer_CreateStructured( &res->blendStats, sizeof( hlsl::BlendStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blendStats" );

	GPUTexture2D_CreateRW( &res->colors, traceDesc->screenWidth, traceDesc->screenHeight, "pixelColors" );
	for ( u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( u32 eye = 0; eye < eyeCount; ++eye )
			GPUTexture2D_CreateRW( &res->histories[bufferID][eye], traceDesc->screenWidth, traceDesc->screenHeight, eye == 0 ? (traceDesc->stereo ? "pixelHistoryLeft" : "pixelHistory") : "pixelHistoryRight" );
	}
	GPUBuffer_CreateTyped( &res->displacements, DXGI_FORMAT_R32_UINT, traceDesc->screenWidth * traceDesc->screenHeight, 0, "pixelDisplacements" );

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		V6_ASSERT_D3D11( g_device->CreateSamplerState( &samplerDesc, &res->bilinearSamplerState ) );
	}

	for ( u32 option = 0; option < 2; ++option )
	{
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket)
			GPUCompute_CreateFromSource( &res->computeCull[option][bucket], hlsl::g_main_block_cull_cs[option][bucket], hlsl::g_sizeof_block_cull_cs[option][bucket] );
	}

	GPUCompute_CreateFromSource( &res->computeTraceInit, hlsl::g_main_block_trace_init_cs, sizeof( hlsl::g_main_block_trace_init_cs ) );
	for ( u32 option = 0; option < 2; ++option )
	{
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket)
			GPUCompute_CreateFromSource( &res->computeTrace[option][bucket], hlsl::g_main_block_trace_cs[option][bucket], hlsl::g_sizeof_block_trace_cs[option][bucket] );
	}

	for ( u32 option = 0; option < 2; ++option )
		GPUCompute_CreateFromSource( &res->computeBlend[option], hlsl::g_main_pixel_blend_cs_options[option], hlsl::g_sizeof_pixel_blend_cs_options[option] );

	GPUCompute_CreateFromSource( &res->computeTSAA, hlsl::g_main_pixel_tsaa_cs, sizeof( hlsl::g_main_pixel_tsaa_cs ) );
	GPUCompute_CreateFromSource( &res->computeSharpen, hlsl::g_main_pixel_sharpen_cs, sizeof( hlsl::g_main_pixel_sharpen_cs ) );

	traceContext->res = res;
	s_gpuTraceResourcesCreated = true;
}

void TraceContext_Release( TraceContext_s* traceContext )
{
	V6_ASSERT( s_gpuTraceResourcesCreated == true );
	GPUTraceResources_s* res = &s_gpuTraceResources;

	const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;

	GPUBuffer_Release( &res->blockPos );
	GPUBuffer_Release( &res->blockData );
	GPUBuffer_Release( &res->ranges[0] );
	GPUBuffer_Release( &res->ranges[1] );
	GPUBuffer_Release( &res->groups[0] );
	GPUBuffer_Release( &res->groups[1] );

	GPUConstantBuffer_Release( &res->cbCull );
	GPUConstantBuffer_Release( &res->cbTrace );
	GPUConstantBuffer_Release( &res->cbBlend );
	GPUConstantBuffer_Release( &res->cbTSAA );

	GPUBuffer_Release( &res->traceCell );
	GPUBuffer_Release( &res->traceIndirectArgs );

	GPUBuffer_Release( &res->cellItems );
	GPUBuffer_Release( &res->cellItemCounters );

	GPUBuffer_Release( &res->cullStats );
	GPUBuffer_Release( &res->traceStats );
	GPUBuffer_Release( &res->traceCellStats );
	GPUBuffer_Release( &res->blendStats );
	
	GPUTexture2D_Release( &res->colors );
	for ( u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( u32 eye = 0; eye < eyeCount; ++eye )
			GPUTexture2D_Release( &res->histories[bufferID][eye] );
	}
	GPUBuffer_Release( &res->displacements );

	V6_RELEASE_D3D11( res->bilinearSamplerState );

	for ( u32 option = 0; option < 2; ++option )
	{
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket)
			GPUCompute_Release( &res->computeCull[option][bucket] );
	}
	GPUCompute_Release( &res->computeTraceInit );
	for ( u32 option = 0; option < 2; ++option )
	{
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket)
			GPUCompute_Release( &res->computeTrace[option][bucket] );
	}
	for ( u32 option = 0; option < 2; ++option )
		GPUCompute_Release( &res->computeBlend[option] );
	GPUCompute_Release( &res->computeTSAA );
	GPUCompute_Release( &res->computeSharpen );

	s_gpuTraceResourcesCreated = false;
}

void TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const View_s* views, const TraceOptions_s* options, IStack* stack )
{
	const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;

	CullBlock( traceContext, views, options );
	TraceBlock( traceContext, views, options, stack );
	for ( u32 eye = 0; eye < eyeCount; ++eye )
	{
		if ( options->noTSAA )
		{
			BlendPixel( traceContext, renderTargetSet->colorBuffers[eye].uav, eye, options );
		}
		else
		{
			BlendPixel( traceContext, traceContext->res->colors.uav, eye, options );
			TSAAPixel( traceContext, renderTargetSet, eye, options );
		}
	}

	traceContext->frameState.resetJitter = false;
}

void TraceContext_GetFrameBasis( TraceContext_s* traceContext, Vec3* origin, float* yaw )
{
	V6_ASSERT( traceContext->frameState.sequenceID < traceContext->stream->desc.sequenceCount );
	const VideoSequence_s* sequence = &traceContext->stream->sequences[traceContext->frameState.sequenceID];

	V6_ASSERT( traceContext->frameState.frameRank < sequence->desc.frameCount );
	const CodecFrameDesc_s* frameDesc = &sequence->frameDescArray[traceContext->frameState.frameRank];
	
	*origin = frameDesc->gridOrigin;
	*yaw = frameDesc->gridYaw;
}

void TraceContext_UpdateFrame( TraceContext_s* traceContext, u32 frameID, IStack* stack )
{
	const u32 sequenceID = frameID / traceContext->stream->desc.playRate;
	const u32 targetFrameRank = frameID % traceContext->stream->desc.playRate;

	const VideoSequence_s* sequence = &traceContext->stream->sequences[sequenceID];

	u32 firstFrameRank;
	if ( sequenceID != traceContext->frameState.sequenceID )
	{
		SequenceContext_Update( &traceContext->sequenceContext, traceContext->stream, sequence );
		traceContext->frameState.frameID = (u32)-1;
		traceContext->frameState.sequenceID = sequenceID;
		firstFrameRank = 0;
	}
	else if ( targetFrameRank > traceContext->frameState.frameRank )
	{
		firstFrameRank = traceContext->frameState.frameRank + 1;
	}
	else if ( targetFrameRank < traceContext->frameState.frameRank )
	{
		firstFrameRank = targetFrameRank;
	}
	else
	{
		return;
	}

	for ( u32 frameRank = firstFrameRank; frameRank <= targetFrameRank; ++frameRank )
	{
		if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		ScopedStack scopedStack( stack );

		Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
		float gridScale = traceContext->stream->desc.gridScaleMin;
		const u32 gridMacroHalfWidth = 1 << (traceContext->stream->desc.gridMacroShift-1);
		for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
			macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameRank].gridOrigin, gridScale, gridMacroHalfWidth ); // patched per frame

		const u16* rangeIDs = sequence->frameDataArray[frameRank].rangeIDs;
		hlsl::BlockRange* const blockRangeBuffer = stack->newArray< hlsl::BlockRange >( CODEC_BUCKET_COUNT * CODEC_RANGE_MAX_COUNT );
		hlsl::BlockRange* blockRanges = blockRangeBuffer;
		u32 blockGroupBuffer[SEQUENCE_BLOCK_GROUP_MAX_COUNT];
		u32* blockGroups = blockGroupBuffer;
		u32 framePosCount = 0;
		u32 frameDataCount = 0;
		u32 frameBlockRangeCount = 0;
		u32 frameGroupCount = 0;
		for ( u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket )
		{
			const u32 cellPerBucketCount = 1 << (bucket + 2);
			const u32 bucketBlockCount = sequence->frameDescArray[frameRank].blockCounts[bucket];
			const u32 bucketBlockRangeCount = sequence->frameDescArray[frameRank].blockRangeCounts[bucket];
			u32 firstThreadID = 0;
			u32 bucketGroupCount = 0;

			for ( u32 rangeRank = 0; rangeRank < bucketBlockRangeCount; ++rangeRank )
			{
				const u32 rangeID = rangeIDs[rangeRank];

				const CodecRange_s* codecRange = &traceContext->sequenceContext.rangeDefs[bucket][rangeID];
				const u32 rangeFrameRank = codecRange->frameRank8_mip4_blockCount20 >> 24;
				const u32 rangeMip = (codecRange->frameRank8_mip4_blockCount20 >> 20) & 0xF;
				const SequenceBlockRange_s* srcBlockRange = &traceContext->sequenceContext.blockRanges[bucket][rangeID];
			
				hlsl::BlockRange* dstBlockRange = &blockRanges[rangeRank];
				dstBlockRange->macroGridOffset = srcBlockRange->macroGridCoords - macroGridCoords[rangeMip];
				dstBlockRange->frameDistance = frameRank - rangeFrameRank;
				dstBlockRange->firstThreadID = firstThreadID;
				dstBlockRange->blockCount = srcBlockRange->blockCount;
				dstBlockRange->blockPosOffset = srcBlockRange->blockPosOffset;
				dstBlockRange->blockDataOffset = srcBlockRange->blockDataOffset;

				const u32 blockCount = srcBlockRange->blockCount;
				const u32 groupCount = HLSL_GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );

				for ( u32 groupRank = 0; groupRank < groupCount; ++groupRank )
					blockGroups[bucketGroupCount + groupRank] = rangeRank;

				firstThreadID += groupCount * HLSL_BLOCK_THREAD_GROUP_SIZE;
				bucketGroupCount += groupCount;
			}

			traceContext->frameState.blockRangeCounts[bucket] = bucketBlockRangeCount;
			traceContext->frameState.groupCounts[bucket] = bucketGroupCount;
			rangeIDs += bucketBlockRangeCount;
			blockRanges += bucketBlockRangeCount;
			blockGroups += bucketGroupCount;
			framePosCount += bucketBlockCount;
			frameDataCount += bucketBlockCount * cellPerBucketCount;
			frameBlockRangeCount += bucketBlockRangeCount;
			frameGroupCount += bucketGroupCount;
		}

		CodecFrameDesc_s* frameDesc = &sequence->frameDescArray[frameRank];
		traceContext->frameState.origin = frameDesc->gridOrigin;
		traceContext->frameState.bufferID = frameID & 1;
	
		GPUTraceResources_s* traceRes = traceContext->res;
		GPUBuffer_Update( &traceRes->ranges[traceContext->frameState.bufferID], 0, blockRangeBuffer, frameBlockRangeCount );
		GPUBuffer_Update( &traceRes->groups[traceContext->frameState.bufferID], 0, blockGroupBuffer, frameGroupCount );
		GPUBuffer_Update( &traceRes->blockPos, traceContext->sequenceContext.frameBlockPosOffsets[frameRank], sequence->frameDataArray[frameRank].blockPos, framePosCount );
		GPUBuffer_Update( &traceRes->blockData, traceContext->sequenceContext.frameBlockDataOffsets[frameRank], sequence->frameDataArray[frameRank].blockData, frameDataCount );
	}

	traceContext->frameState.resetJitter = traceContext->frameState.frameID + 1 != frameID;

	traceContext->frameState.frameRank = targetFrameRank;
	traceContext->frameState.frameID = frameID;
}

END_V6_NAMESPACE
