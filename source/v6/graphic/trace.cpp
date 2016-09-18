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

BEGIN_V6_NAMESPACE

struct GPUTraceResources_s
{
	GPUConstantBuffer_s		cbCull;
	GPUConstantBuffer_s		cbProject;
	GPUConstantBuffer_s		cbTrace;
	GPUConstantBuffer_s		cbTSAA;
	GPUConstantBuffer_s		cbPostProcess;

	GPUBuffer_s				blockPos;
	GPUBuffer_s				blockCellPresences0;
	GPUBuffer_s				blockCellPresences1;
	GPUBuffer_s				blockCellEndColors;
	GPUBuffer_s				blockCellColorIndices0;
	GPUBuffer_s				blockCellColorIndices1;
	GPUBuffer_s				blockCellColorIndices2;
	GPUBuffer_s				blockCellColorIndices3;
	GPUBuffer_s				ranges[2];
	GPUBuffer_s				groups[2];

	GPUBuffer_s				visibleBlocks;
	GPUBuffer_s				visibleBlockContext;

	GPUBuffer_s				blockPatchCounters;
	GPUBuffer_s				blockPatches;

	GPUTexture2D_s			colors;
	GPUTexture2D_s			histories[2][2];
	GPUBuffer_s				displacements;

	ID3D11SamplerState*		bilinearSamplerState;

	GPUBuffer_s				cullStats;
	GPUBuffer_s				projectStats;
	GPUBuffer_s				traceStats;
	GPUBuffer_s				traceDebugBoxes;

	GPUCompute_s			computeCull[4];
	GPUCompute_s			computeCullPost;
	GPUCompute_s			computeProject[4];
	GPUCompute_s			computeTrace[4];
	GPUCompute_s			computeTSAA;
	GPUCompute_s			computeSharpen;
};

static const GPUEventID_t s_gpuEventCull			= GPUEvent_Register( "Cull", true );
static const GPUEventID_t s_gpuEventCullBucket		= GPUEvent_Register( "Cull Bucket", false );
static const GPUEventID_t s_gpuEventProjects[2]		= { GPUEvent_Register( "Project_L", true ), GPUEvent_Register( "Project_R", true ) };
static const GPUEventID_t s_gpuEventTraces[2]		= { GPUEvent_Register( "Trace_L", true ), GPUEvent_Register( "Trace_R", true ) };
static const GPUEventID_t s_gpuEventTSAAs[2]		= { GPUEvent_Register( "TSAA_L", true ), GPUEvent_Register( "TSAA_R", true ) };
static const GPUEventID_t s_gpuEventSharpens[2]		= { GPUEvent_Register( "Sharpen_L", true ), GPUEvent_Register( "Sharpen_R", true ) };

extern ID3D11Device*							g_device;
extern ID3D11DeviceContext*						g_deviceContext;

static GPUTraceResources_s						s_gpuTraceResources;
static bool										s_gpuTraceResourcesCreated = false;

static const u32 SEQUENCE_BLOCK_GROUP_MAX_COUNT = 65536;

static u32 GetShaderOption( TraceContext_s* traceContext, bool debug )
{
	u32 shaderOption = 0;
	shaderOption += (traceContext->stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW) == 0 ? 1 : 0;
	shaderOption += debug ? 2 : 0;
	return shaderOption;
}

static void CullBlock( TraceContext_s* traceContext, const View_s* views, const TraceOptions_s* options )
{
	static const void* nulls[8] = {};

	GPUTraceResources_s* traceRes = traceContext->res;

	GPUEvent_Begin( s_gpuEventCull );

	// Clear

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( traceRes->visibleBlockContext.uav, values );
	if ( options->logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->cullStats.uav, values );

	// Update buffers

	const u32 gridMacroHalfWidth = traceContext->stream->desc.gridWidth >> 3;
	const float invGridWidth = 1.0f / traceContext->stream->desc.gridWidth;
	const u32 leftEye = 0;
	const u32 rightEye = traceContext->desc.stereo ? 1 : 0;

	{
		v6::hlsl::CBCull* cbCull = (v6::hlsl::CBCull*)GPUConstantBuffer_MapWrite( &traceRes->cbCull );

		cbCull->c_cullInvGridWidth = invGridWidth;
		cbCull->c_cullFrameChanged = traceContext->frameState.frameChanged || options->showHistory;

		if ( traceContext->stream->desc.flags == CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		{
			float gridScale = traceContext->stream->desc.gridScaleMin;
			for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			{
				const Vec3 center = Codec_ComputeGridCenter( &traceContext->frameState.origin, gridScale, gridMacroHalfWidth );
				cbCull->c_cullCentersAndGridScales[gridID] = Vec4_Make( &center, gridScale );
				if ( gridScale < traceContext->stream->desc.gridScaleMax )
					gridScale *= 2;
			}
		}
		else
		{
			const u32 gridMacroWidth = traceContext->stream->desc.gridWidth >> 2;
			const float invMacroPeriodWidth = log2f( 1.0f + 2.0f / gridMacroWidth );

			cbCull->c_cullGridMinScale = traceContext->stream->desc.gridScaleMin;
			cbCull->c_cullInvMacroPeriodWidth = invMacroPeriodWidth;
			cbCull->c_cullInvMacroGridWidth = 1.0f / gridMacroWidth;

			cbCull->c_cullGridCenter = Vec4_Make( &traceContext->stream->desc.gridOrigin, 0.0f );
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
		
		cbCull->c_cullFrustumPlanes[0] = Vec4_Make( &leftPlane, Dot( leftPlane, views[leftEye].org ) );
		cbCull->c_cullFrustumPlanes[1] = Vec4_Make( &rightPlane, Dot( rightPlane, views[rightEye].org ) );
		cbCull->c_cullFrustumPlanes[2] = Vec4_Make( &upPlane, Dot( upPlane, views[0].org ) );
		cbCull->c_cullFrustumPlanes[3] = Vec4_Make( &bottomPlane, Dot( bottomPlane, views[0].org ) );

		GPUConstantBuffer_UnmapWrite( &traceRes->cbCull );
	}

	// set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBCullSlot, 1, &traceRes->cbCull.buf );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, &traceRes->groups[traceContext->frameState.bufferID].srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, &traceRes->ranges[traceContext->frameState.bufferID].srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &traceRes->blockPos.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_VISIBLE_BLOCK_SLOT, 1, &traceRes->visibleBlocks.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT, 1, &traceRes->visibleBlockContext.uav, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, &traceRes->cullStats.uav, nullptr );

	// dispach
	const u32 shaderOption = GetShaderOption( traceContext, options->logReadBack);
	GPUCompute_Dispatch( &traceRes->computeCull[shaderOption], traceContext->frameState.groupCount, 1, 1 );
	
	// unset

	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_GROUP_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_RANGE_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_VISIBLE_BLOCK_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	// post

	{
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT, 1, &traceRes->visibleBlockContext.uav, nullptr );

		GPUCompute_Dispatch( &traceRes->computeCullPost, 1, 1, 1 );

		g_deviceContext->CSSetUnorderedAccessViews( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	}

	GPUEvent_End();

	if ( options->logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlockCullStats* blockCullStats = (hlsl::BlockCullStats*)GPUBuffer_MapReadBack( &traceRes->cullStats );

			ReadBack_Log( "blockCull", blockCullStats->blockInputCount, "blockInputCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockProcessedCount, "blockProcessedCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockPassedCount, "blockPassedCount" );
			V6_ASSERT( blockCullStats->blockPassedCount <= traceContext->resVisibleBlockMaxCount );

			if ( blockCullStats->assertFailedBits )
			{
				ReadBack_Log( "blockCullDebug", hex32 { blockCullStats->assertFailedBits }, "assertFailedBits" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataU32[0], "assertDataU32[0]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataU32[1], "assertDataU32[1]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataU32[2], "assertDataU32[2]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataU32[3], "assertDataU32[3]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataF32[0], "assertDataF32[0]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataF32[1], "assertDataF32[1]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataF32[2], "assertDataF32[2]" );
				ReadBack_Log( "blockCullDebug", blockCullStats->assertDataF32[3], "assertDataF32[3]" );
				V6_ASSERT_ALWAYS( "HLSL Assert" );
			}

			GPUBuffer_UnmapReadBack( &traceRes->cullStats );
		}
	}
}

static void ProjectBlock( TraceContext_s* traceContext, u32 eye, const TraceOptions_s* options )
{
	static const void* nulls[8] = {};

	GPUTraceResources_s* traceRes = traceContext->res;

	GPUEvent_Begin( s_gpuEventProjects[eye] );

	// Clear

	const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( traceRes->blockPatchCounters.uav, values );
	if ( options->logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->projectStats.uav, values );

	// Update buffers

	const u32 gridMacroHalfWidth = traceContext->stream->desc.gridWidth >> 3;
	const float invGridWidth = 1.0f / traceContext->stream->desc.gridWidth;

	{
		v6::hlsl::CBProject* cbProject = (v6::hlsl::CBProject*)GPUConstantBuffer_MapWrite( &traceRes->cbProject );

		cbProject->c_projectFrameSize = Vec2_Make( (float)traceContext->desc.screenWidth, (float)traceContext->desc.screenHeight );
		cbProject->c_projectFrameTileSize = Vec2u_Make( traceContext->desc.screenWidth >> 3, traceContext->desc.screenHeight >> 3 );
		
		cbProject->c_projectInvGridWidth = invGridWidth;

		if ( traceContext->stream->desc.flags == CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		{
			float gridScale = traceContext->stream->desc.gridScaleMin;
			for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			{
				const Vec3 center = Codec_ComputeGridCenter( &traceContext->frameState.origin, gridScale, gridMacroHalfWidth );
				cbProject->c_projectCentersAndGridScales[gridID] = Vec4_Make( &center, gridScale );
				if ( gridScale < traceContext->stream->desc.gridScaleMax )
					gridScale *= 2;
			}
		}
		else
		{
			const u32 gridMacroWidth = traceContext->stream->desc.gridWidth >> 2;
			const float invMacroPeriodWidth = log2f( 1.0f + 2.0f / gridMacroWidth );

			cbProject->c_projectGridMinScale = traceContext->stream->desc.gridScaleMin;
			cbProject->c_projectInvMacroPeriodWidth = invMacroPeriodWidth;
			cbProject->c_projectInvMacroGridWidth = 1.0f / gridMacroWidth;
			
			cbProject->c_projectGridCenter = Vec4_Make( &traceContext->stream->desc.gridOrigin, 0.0f );
		}

		cbProject->c_projectPrevWorldToProjX = traceContext->frameState.prevWorldToProjsX[eye];
		cbProject->c_projectPrevWorldToProjY = traceContext->frameState.prevWorldToProjsY[eye];
		cbProject->c_projectPrevWorldToProjW = traceContext->frameState.prevWorldToProjsW[eye];
		cbProject->c_projectCurWorldToProjX = traceContext->frameState.curWorldToProjsX[eye];
		cbProject->c_projectCurWorldToProjY = traceContext->frameState.curWorldToProjsY[eye];
		cbProject->c_projectCurWorldToProjW = traceContext->frameState.curWorldToProjsW[eye];

		GPUConstantBuffer_UnmapWrite( &traceRes->cbProject );
	}

	// set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBProjectSlot, 1, &traceRes->cbProject.buf );
	g_deviceContext->CSSetShaderResources( HLSL_VISIBLE_BLOCK_SLOT, 1, &traceRes->visibleBlocks.srv );
	g_deviceContext->CSSetShaderResources( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT, 1, &traceRes->visibleBlockContext.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE0_SLOT, 1, &traceRes->blockCellPresences0.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE1_SLOT, 1, &traceRes->blockCellPresences1.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_PATCH_COUNTERS_SLOT, 1, &traceRes->blockPatchCounters.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_PATCHES_SLOT, 1, &traceRes->blockPatches.uav, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_PROJECT_STATS_SLOT, 1, &traceRes->projectStats.uav, nullptr );

	// dispach
	
	const u32 shaderOption = GetShaderOption( traceContext, options->logReadBack);
	GPUCompute_DispatchIndirect( &traceRes->computeProject[shaderOption], &traceRes->visibleBlockContext, offsetof( hlsl::VisibleBlockContext, groupCountX ) );
	
	// Unset
	g_deviceContext->CSSetShaderResources( HLSL_VISIBLE_BLOCK_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_VISIBLE_BLOCK_CONTEXT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE0_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE1_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_PATCH_COUNTERS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_PATCHES_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( options->logReadBack )
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_PROJECT_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPUEvent_End();

	if ( options->logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlockProjectStats* blockProjectStats = (hlsl::BlockProjectStats*)GPUBuffer_MapReadBack( &traceRes->projectStats );

			ReadBack_Log( "blockProject", blockProjectStats->blockInputCount, "blockInputCount" );
			ReadBack_Log( "blockProject", blockProjectStats->blockProcessedCount, "blockProcessedCount" );
			ReadBack_Log( "blockProject", blockProjectStats->blockPatchHeaderPixelCount, "blockPatchHeaderPixelCount" );
			ReadBack_Log( "blockProject", blockProjectStats->blockPatchHeaderCount, "blockPatchHeaderCount" );
			ReadBack_Log( "blockProject", blockProjectStats->blockPatchDetailCount, "blockPatchDetailCount" );
			ReadBack_Log( "blockProject", blockProjectStats->blockPatchDetailPixelCount, "blockPatchDetailPixelCount" );
			ReadBack_Log( "blockProject", blockProjectStats->blockPatchMaxPage, "blockPatchMaxPage" );
			// V6_ASSERT( blockProjectStats->blockPatchHeaderPixelCount == blockProjectStats->blockPatchDetailPixelCount );

			GPUBuffer_UnmapReadBack( &traceRes->projectStats );
		}
	}
}

static void TraceBlock( TraceContext_s* traceContext, ID3D11UnorderedAccessView* outputColors, u32 eye, const View_s* views, const TraceOptions_s* options )
{
	static const void* nulls[8] = {};

	const Vec2u frameTileSize = Vec2u_Make( traceContext->desc.screenWidth >> 3, traceContext->desc.screenHeight >> 3 );

	GPUTraceResources_s* traceRes = traceContext->res;

	GPUEvent_Begin( s_gpuEventTraces[eye] );

	u32 values[4] = {};
	if ( options->logReadBack )
		g_deviceContext->ClearUnorderedAccessViewUint( traceRes->traceStats.uav, values );

	{
		const u32 gridMacroHalfWidth = traceContext->stream->desc.gridWidth >> 3;
		const float invGridWidth = 1.0f / traceContext->stream->desc.gridWidth;
		
		const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;

		v6::hlsl::CBTrace* cbTrace = (v6::hlsl::CBTrace*)GPUConstantBuffer_MapWrite( &traceRes->cbTrace );


		if ( traceContext->stream->desc.flags == CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		{
			float gridScale = traceContext->stream->desc.gridScaleMin;
			for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			{
				const float cellSize = gridScale * 2.0f * invGridWidth;
				cbTrace->c_traceGridScales[gridID] = Vec4_Make( gridScale, cellSize, 1.0f / cellSize, 0.0f );
				const Vec3 center = Codec_ComputeGridCenter( &traceContext->frameState.origin, gridScale, gridMacroHalfWidth );
				cbTrace->c_traceGridCenters[gridID] = Vec4_Make( &center, 0.0f );
				if ( gridScale < traceContext->stream->desc.gridScaleMax )
					gridScale *= 2;
			}
		}
		else
		{
			const u32 gridMacroWidth = traceContext->stream->desc.gridWidth >> 2;
			const float invMacroPeriodWidth = log2f( 1.0f + 2.0f / gridMacroWidth );

			cbTrace->c_traceGridMinScale = traceContext->stream->desc.gridScaleMin;
			cbTrace->c_traceInvMacroPeriodWidth = invMacroPeriodWidth;
			cbTrace->c_traceInvMacroGridWidth = 1.0f / gridMacroWidth;

			cbTrace->c_traceGridCenter = Vec4_Make( &traceContext->stream->desc.gridOrigin, 0.0f );
		}

		cbTrace->c_traceEyeCount = eyeCount;

		const Vec2 frameSize = Vec2_Make( (float)traceContext->desc.screenWidth, (float)traceContext->desc.screenHeight );
		cbTrace->c_traceFrameSize = frameSize;
		cbTrace->c_traceFrameTileSize = frameTileSize;

		const float scaleRight = (views[eye].tanHalfFOVLeft + views[eye].tanHalfFOVRight) / frameSize.x;
		const float scaleUp = (views[eye].tanHalfFOVUp + views[eye].tanHalfFOVDown) / frameSize.y;

		const Vec3 forward = views[eye].forward;
		const Vec3 right = views[eye].right * scaleRight;
		const Vec3 up = views[eye].up * scaleUp;
		const Vec3 base = forward - views[eye].up * views[eye].tanHalfFOVDown - views[eye].right * views[eye].tanHalfFOVLeft;
				
		cbTrace->c_traceRayOrg = Vec4_Make( &views[eye].org, 0.0f );
		cbTrace->c_traceRayDirBase = Vec4_Make( &base, 0.0f );
		cbTrace->c_traceRayDirUp = Vec4_Make( &up, 0.0f );
		cbTrace->c_traceRayDirRight = Vec4_Make( &right, 0.0f );

		cbTrace->c_traceGetStats = options->logReadBack;
		cbTrace->c_traceShowFlag = (options->showGrid ? HLSL_BLOCK_SHOW_FLAG_GRIDS : 0) | (options->showHistory ? HLSL_BLOCK_SHOW_FLAG_HISTORY : 0) | (options->showOverdraw ? HLSL_BLOCK_SHOW_FLAG_OVERDRAW : 0) | (options->showBlock ? HLSL_BLOCK_SHOW_FLAG_BLOCK : 0);

		cbTrace->c_traceJitter = traceContext->frameState.jitter;

		GPUConstantBuffer_UnmapWrite( &traceRes->cbTrace );
	}

	// set

	g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBTraceSlot, 1, &traceRes->cbTrace.buf );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_PATCH_COUNTERS_SLOT, 1, &traceRes->blockPatchCounters.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_PATCHES_SLOT, 1, &traceRes->blockPatches.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE0_SLOT, 1, &traceRes->blockCellPresences0.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE1_SLOT, 1, &traceRes->blockCellPresences1.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_END_COLOR_SLOT, 1, &traceRes->blockCellEndColors.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX0_SLOT, 1, &traceRes->blockCellColorIndices0.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX1_SLOT, 1, &traceRes->blockCellColorIndices1.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX2_SLOT, 1, &traceRes->blockCellColorIndices2.srv );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX3_SLOT, 1, &traceRes->blockCellColorIndices3.srv );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, &outputColors, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_DISPLACEMENT_SLOT, 1, &traceRes->displacements.uav, nullptr );
	if ( options->logReadBack )
	{
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, &traceRes->traceStats.uav, nullptr );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_DEBUG_BOX_SLOT, 1, &traceRes->traceDebugBoxes.uav, nullptr );
	}

	// dispach
	
	const u32 shaderOption = GetShaderOption( traceContext, options->logReadBack || options->showGrid || options->showHistory || options->showOverdraw || options->showBlock );
	GPUCompute_Dispatch( &traceRes->computeTrace[shaderOption], frameTileSize.x, frameTileSize.y, 1 );

	// Unset

	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_PATCH_COUNTERS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_PATCHES_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE0_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_PRESENCE1_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_END_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX0_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX1_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX2_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_COLOR_INDEX3_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_COLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( HLSL_DISPLACEMENT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( options->logReadBack )
	{
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
		g_deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_DEBUG_BOX_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	}
	
	GPUEvent_End();

	if ( options->logReadBack )
	{			
		V6_MSG( "\n" );

		{
			const hlsl::BlockTraceStats* blockTraceStats = (hlsl::BlockTraceStats*)GPUBuffer_MapReadBack( &traceRes->traceStats );

			u32 tileInputCount = 0;
			for ( u32 page = 0; page < HLSL_BLOCK_PAGE_MAX_COUNT; ++page )
			{
				if ( blockTraceStats->tileInputCounts[page] )
				{
					ReadBack_Log( "blockTrace", blockTraceStats->tileInputCounts[page], String_Format( "tileInputCount_with_%d_pages", page + 1 ) );
					tileInputCount += blockTraceStats->tileInputCounts[page];
				}
			}
			ReadBack_Log( "blockTrace", tileInputCount, "tileInputCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->patchInputCount, "patchInputCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelInputCount, "pixelInputCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelTraceCount, "pixelTraceCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelEmptyMaskCount, "pixelMaskCount_empty" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelNotEmptyMaskCount, "pixelMaskCount_not_empty" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelHitCounts[0], "pixelHitCount_hit" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelHitCounts[1], "pixelHitCount_miss_block" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelHitCounts[2], "pixelHitCount_miss_cell" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelDoneCount, "pixelDoneCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelPageCount, "pixelPageCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelStepCount, "pixelStepCount" );
			if ( blockTraceStats->assertFailedBits )
			{
				ReadBack_Log( "blockTrace", hex32 { blockTraceStats->assertFailedBits }, "assertFailedBits" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataU32[0], "assertDataU32[0]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataU32[1], "assertDataU32[1]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataU32[2], "assertDataU32[2]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataU32[3], "assertDataU32[3]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataF32[0], "assertDataF32[0]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataF32[1], "assertDataF32[1]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataF32[2], "assertDataF32[2]" );
				ReadBack_Log( "blockTrace", blockTraceStats->assertDataF32[3], "assertDataF32[3]" );
				V6_ASSERT_ALWAYS( "HLSL Assert" );
			}

			if ( blockTraceStats->debugBoxCount > 0 )
			{
				const hlsl::BlockDebugBox* blockDebugBoxes = (hlsl::BlockDebugBox*)GPUBuffer_MapReadBack( &traceRes->traceDebugBoxes );

				Plot_s plot;
				Plot_Create( &plot, "d:/tmp/plot/testTrace" );
				
				Vec3 center = Vec3_Zero();
				for ( u32 boxID = 0; boxID < blockTraceStats->debugBoxCount; ++boxID )
				{
					center += blockDebugBoxes[boxID].boxMinRS;
					center += blockDebugBoxes[boxID].boxMaxRS;
					Plot_AddBox( &plot, &blockDebugBoxes[boxID].boxMinRS, &blockDebugBoxes[boxID].boxMaxRS, false );
					Plot_AddBox( &plot, &blockDebugBoxes[boxID].boxMinRS, &blockDebugBoxes[boxID].boxMaxRS, true );
				}
				center *= 1.0f / (blockTraceStats->debugBoxCount * 2.0f);

				const Vec3 rayDir = blockTraceStats->debugRayDir * center.Length() * 2.0f;
				const Vec3 zero = Vec3_Zero();
				Plot_AddLine( &plot, &zero, &rayDir );

				Plot_Release( &plot );

				GPUBuffer_UnmapReadBack( &traceRes->traceDebugBoxes );
			}

			GPUBuffer_UnmapReadBack( &traceRes->traceStats );
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
	
		// Update

		{
			v6::hlsl::CBPostProcess* cbPostProcess = (v6::hlsl::CBPostProcess*)GPUConstantBuffer_MapWrite( &traceRes->cbPostProcess );

			cbPostProcess->c_postProcessFadeToBlack = traceContext->frameState.fadeToBlack;

			GPUConstantBuffer_UnmapWrite( &traceRes->cbPostProcess );
		}

		// Set

		g_deviceContext->CSSetConstantBuffers( v6::hlsl::CBPostProcessSlot, 1, &traceRes->cbPostProcess.buf );
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

static void Frame_ComputeMacroGridCoords( Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT], u32 frameRank, const VideoStream_s* stream, const VideoSequence_s* sequence )
{
	float gridScale = stream->desc.gridScaleMin;
	const u32 gridMacroHalfWidth = stream->desc.gridWidth >> 3;
	for ( u32 mip = 0; mip < CODEC_MIP_MAX_COUNT; ++mip, gridScale *= 2.0f )
		macroGridCoords[mip] = Codec_ComputeMacroGridCoords( &sequence->frameDescArray[frameRank].gridOrigin, gridScale, gridMacroHalfWidth );
}

static void SequenceContext_Update( SequenceContext_s* sequenceContext, const VideoStream_s* stream, const VideoSequence_s* sequence )
{
	memset( sequenceContext, 0, sizeof( *sequenceContext ) );

	u32 nextRangeID = 0;
	u32 blockPosCount = 0;
	for ( u32 frameRank = 0; frameRank < sequence->desc.frameCount; ++frameRank )
	{
		if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		sequenceContext->frameBlockPosOffsets[frameRank] = blockPosCount;

		Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
		if ( stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
			Frame_ComputeMacroGridCoords( macroGridCoords, frameRank, stream, sequence );
		
		for (;;)
		{
			const u32 rangeID = nextRangeID;

			if ( rangeID == sequence->desc.rangeDefCount )
				break;
			
			const CodecRange_s* codecRange = &sequence->data.rangeDefs[rangeID];
			const u32 rangeFrameRank = codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 25;
			if ( frameRank != rangeFrameRank )
				break;

			const u32 isNewBlock = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 24) & 1;
			const u32 grid = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 20) & 0xF;
			const u32 blockCount = codecRange->frameRank7_newBlock1_grid4_blockCount20 & 0xFFFFF;

			SequenceBlockRange_s* blockRange = &sequenceContext->blockRanges[rangeID];
			
			blockRange->macroGridCoords = macroGridCoords[grid];
			blockRange->blockCount = blockCount;
			blockRange->blockPosOffset = blockPosCount;
			blockRange->isNewBlock = isNewBlock;

			blockPosCount += blockCount;

			++nextRangeID;
		}
	}

	V6_ASSERT( nextRangeID == sequence->desc.rangeDefCount );
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

	const u32 blockCount = Max( 1u, stream->desc.maxBlockCountPerSequence );

	const u32 maxBlockRangeCount = Max( 1u, stream->desc.maxBlockRangeCountPerFrame );
	const u32 maxBlockGroupCount = Max( 1u, stream->desc.maxBlockGroupCountPerFrame );

	V6_ASSERT( maxBlockGroupCount <= SEQUENCE_BLOCK_GROUP_MAX_COUNT );

	const u32 gpuBufferFlag = GPUBUFFER_CREATION_FLAG_MAP_NO_OVERWRITE;
	GPUBuffer_CreateTyped( &res->blockPos, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockPositions" );
	GPUBuffer_CreateTyped( &res->blockCellPresences0, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellPresences0" );
	GPUBuffer_CreateTyped( &res->blockCellPresences1, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellPresences1" );
	GPUBuffer_CreateTyped( &res->blockCellEndColors, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellEndColors" );
	GPUBuffer_CreateTyped( &res->blockCellColorIndices0, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellColorIndices0" );
	GPUBuffer_CreateTyped( &res->blockCellColorIndices1, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellColorIndices1" );
	GPUBuffer_CreateTyped( &res->blockCellColorIndices2, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellColorIndices2" );
	GPUBuffer_CreateTyped( &res->blockCellColorIndices3, DXGI_FORMAT_R32_UINT, blockCount, gpuBufferFlag, "sequenceBlockCellColorIndices3" );
	GPUBuffer_CreateStructured( &res->ranges[0], sizeof( hlsl::BlockRange ), maxBlockRangeCount, gpuBufferFlag, "sequenceBlockRanges0" );
	GPUBuffer_CreateStructured( &res->ranges[1], sizeof( hlsl::BlockRange ), maxBlockRangeCount, gpuBufferFlag, "sequenceBlockRanges1" );
	GPUBuffer_CreateTyped( &res->groups[0], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, gpuBufferFlag, "sequenceBlockGroups0" );
	GPUBuffer_CreateTyped( &res->groups[1], DXGI_FORMAT_R32_UINT, maxBlockGroupCount, gpuBufferFlag, "sequenceBlockGroups1" );
	
	const u32 eyeCount = traceDesc->stereo ? 2 : 1;
	const u32 blockTileCountPerEye = (traceDesc->screenWidth >> 3) * (traceDesc->screenHeight >> 3);
	traceContext->resVisibleBlockMaxCount = maxBlockGroupCount * CODEC_BLOCK_THREAD_GROUP_SIZE;
	traceContext->resBlockPatchCountPerEye = blockTileCountPerEye * HLSL_BLOCK_PATCH_MAX_COUNT_PER_TILE;

	GPUConstantBuffer_Create( &res->cbCull, sizeof( v6::hlsl::CBCull ), "cull" );
	GPUConstantBuffer_Create( &res->cbProject, sizeof( v6::hlsl::CBProject ), "project" );
	GPUConstantBuffer_Create( &res->cbTrace, sizeof( v6::hlsl::CBTrace ), "trace" );
	GPUConstantBuffer_Create( &res->cbTSAA, sizeof( v6::hlsl::CBTSAA), "tsaa" );
	GPUConstantBuffer_Create( &res->cbPostProcess, sizeof( v6::hlsl::CBTSAA), "postProcess" );

	if ( stream->desc.flags == CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		GPUBuffer_CreateStructured( &res->visibleBlocks, sizeof( hlsl::VisibleBlockMip ), traceContext->resVisibleBlockMaxCount, 0, "visibleBlocks" );
	else
		GPUBuffer_CreateStructured( &res->visibleBlocks, sizeof( hlsl::VisibleBlockOnion ), traceContext->resVisibleBlockMaxCount, 0, "visibleBlocks" );
	GPUBuffer_CreateIndirectArgs( &res->visibleBlockContext, sizeof( hlsl::VisibleBlockContext ) / sizeof( u32 ), 0, "visibleBlockContext" );

	GPUBuffer_CreateTyped( &res->blockPatchCounters, DXGI_FORMAT_R32_UINT, blockTileCountPerEye, 0, "blockPatchCounters" );
	if ( stream->desc.flags == CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
		GPUBuffer_CreateStructured( &res->blockPatches, sizeof( hlsl::BlockPatchMip ), traceContext->resBlockPatchCountPerEye, 0, "blockPatches" );
	else
		GPUBuffer_CreateStructured( &res->blockPatches, sizeof( hlsl::BlockPatchOnion ), traceContext->resBlockPatchCountPerEye, 0, "blockPatches" );
	
	GPUBuffer_CreateStructured( &res->cullStats, sizeof( hlsl::BlockCullStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockCullStats" );
	GPUBuffer_CreateStructured( &res->projectStats, sizeof( hlsl::BlockProjectStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockProjectStats" );
	GPUBuffer_CreateStructured( &res->traceStats, sizeof( hlsl::BlockTraceStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockTraceStats" );
	GPUBuffer_CreateStructured( &res->traceDebugBoxes, sizeof( hlsl::BlockDebugBox ), HLSL_BLOCK_DEBUG_BOX_MAX_COUNT, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockDebugBox" );

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

	for ( u32 option = 0; option < 4; ++option )
		GPUCompute_CreateFromSource( &res->computeCull[option], hlsl::g_main_block_cull_cs[option], hlsl::g_sizeof_block_cull_cs[option] );
	GPUCompute_CreateFromSource( &res->computeCullPost, hlsl::g_main_block_cull_post_cs, sizeof( hlsl::g_main_block_cull_post_cs ) );

	for ( u32 option = 0; option < 4; ++option )
		GPUCompute_CreateFromSource( &res->computeProject[option], hlsl::g_main_block_project_cs[option], hlsl::g_sizeof_block_project_cs[option] );

	for ( u32 option = 0; option < 4; ++option )
		GPUCompute_CreateFromSource( &res->computeTrace[option], hlsl::g_main_block_trace_cs[option], hlsl::g_sizeof_block_trace_cs[option] );

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
	GPUBuffer_Release( &res->blockCellPresences0 );
	GPUBuffer_Release( &res->blockCellPresences1 );
	GPUBuffer_Release( &res->blockCellEndColors );
	GPUBuffer_Release( &res->blockCellColorIndices0 );
	GPUBuffer_Release( &res->blockCellColorIndices1 );
	GPUBuffer_Release( &res->blockCellColorIndices2 );
	GPUBuffer_Release( &res->blockCellColorIndices3 );
	GPUBuffer_Release( &res->ranges[0] );
	GPUBuffer_Release( &res->ranges[1] );
	GPUBuffer_Release( &res->groups[0] );
	GPUBuffer_Release( &res->groups[1] );

	GPUConstantBuffer_Release( &res->cbCull );
	GPUConstantBuffer_Release( &res->cbProject );
	GPUConstantBuffer_Release( &res->cbTrace );
	GPUConstantBuffer_Release( &res->cbPostProcess );
	
	GPUBuffer_Release( &res->visibleBlocks );
	GPUBuffer_Release( &res->visibleBlockContext );

	GPUBuffer_Release( &res->blockPatchCounters );
	GPUBuffer_Release( &res->blockPatches );

	GPUBuffer_Release( &res->cullStats );
	GPUBuffer_Release( &res->projectStats );
	GPUBuffer_Release( &res->traceStats );
	GPUBuffer_Release( &res->traceDebugBoxes );
	
	GPUTexture2D_Release( &res->colors );
	for ( u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( u32 eye = 0; eye < eyeCount; ++eye )
			GPUTexture2D_Release( &res->histories[bufferID][eye] );
	}
	GPUBuffer_Release( &res->displacements );

	V6_RELEASE_D3D11( res->bilinearSamplerState );

	for ( u32 option = 0; option < 4; ++option )
		GPUCompute_Release( &res->computeCull[option] );
	GPUCompute_Release( &res->computeCullPost );
	for ( u32 option = 0; option < 4; ++option )
		GPUCompute_Release( &res->computeProject[option] );
	for ( u32 option = 0; option < 4; ++option )
		GPUCompute_Release( &res->computeTrace[option] );
	GPUCompute_Release( &res->computeTSAA );
	GPUCompute_Release( &res->computeSharpen );

	s_gpuTraceResourcesCreated = false;
}

void TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const View_s* views, const TraceOptions_s* options, float fadeToBlack )
{
	const u32 eyeCount = traceContext->desc.stereo ? 2 : 1;

	
	for ( u32 eye = 0; eye < eyeCount; ++eye )
	{
		Mat4x4 worldToProj;
		Mat4x4_Mul( &worldToProj, views[eye].projMatrix, views[eye].viewMatrix );

		traceContext->frameState.curWorldToProjsX[eye] = worldToProj.m_row0;
		traceContext->frameState.curWorldToProjsY[eye] = worldToProj.m_row1;
		traceContext->frameState.curWorldToProjsW[eye] = worldToProj.m_row3;
	}

	traceContext->frameState.fadeToBlack = fadeToBlack;

	if ( traceContext->frameState.resetJitter )
	{
		for ( u32 eye = 0; eye < eyeCount; ++eye )
		{
			traceContext->frameState.prevWorldToProjsX[eye] = traceContext->frameState.curWorldToProjsX[eye];
			traceContext->frameState.prevWorldToProjsY[eye] = traceContext->frameState.curWorldToProjsY[eye];
			traceContext->frameState.prevWorldToProjsW[eye] = traceContext->frameState.curWorldToProjsW[eye];
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

	CullBlock( traceContext, views, options );
	for ( u32 eye = 0; eye < eyeCount; ++eye )
	{
		ProjectBlock( traceContext, eye, options );

		if ( options->noTSAA )
		{
			TraceBlock( traceContext, renderTargetSet->colorBuffers[eye].uav, eye, views, options );
		}
		else
		{
			TraceBlock( traceContext, traceContext->res->colors.uav, eye, views, options );
			TSAAPixel( traceContext, renderTargetSet, eye, options );
		}
	}

	for ( u32 eye = 0; eye < eyeCount; ++eye )
	{
		traceContext->frameState.prevWorldToProjsX[eye] = traceContext->frameState.curWorldToProjsX[eye];
		traceContext->frameState.prevWorldToProjsY[eye] = traceContext->frameState.curWorldToProjsY[eye];
		traceContext->frameState.prevWorldToProjsW[eye] = traceContext->frameState.curWorldToProjsW[eye];
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
	const u32 sequenceID = VideoStream_FindSequenceIDFromFrameID( traceContext->stream, frameID );
	const u32 targetFrameRank = frameID - traceContext->stream->frameOffets[sequenceID];

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
		traceContext->frameState.frameChanged = false;
		return;
	}

	for ( u32 frameRank = firstFrameRank; frameRank <= targetFrameRank; ++frameRank )
	{
		if ( sequence->frameDescArray[frameRank].flags & CODEC_FRAME_FLAG_MOTION )
			continue;

		ScopedStack scopedStack( stack );

		Vec3i macroGridCoords[CODEC_MIP_MAX_COUNT] = {};
		if ( traceContext->stream->desc.flags & CODEC_STREAM_FLAG_MOVING_POINT_OF_VIEW )
			Frame_ComputeMacroGridCoords( macroGridCoords, frameRank, traceContext->stream, sequence );

		const u16* rangeIDs = sequence->frameDataArray[frameRank].rangeIDs;
		hlsl::BlockRange* const blockRangeBuffer = stack->newArray< hlsl::BlockRange >( CODEC_RANGE_MAX_COUNT );
		hlsl::BlockRange* blockRanges = blockRangeBuffer;
		u32 blockGroupBuffer[SEQUENCE_BLOCK_GROUP_MAX_COUNT];
		u32* blockGroups = blockGroupBuffer;
		const u32 frameBlockCount = sequence->frameDescArray[frameRank].blockCount;
		const u32 frameBlockRangeCount = sequence->frameDescArray[frameRank].blockRangeCount;
		
		u32 frameGroupCount = 0;
		u32 firstThreadID = 0;
		for ( u32 rangeRank = 0; rangeRank < frameBlockRangeCount; ++rangeRank )
		{
			const u32 rangeID = rangeIDs[rangeRank];

			const CodecRange_s* codecRange = &sequence->data.rangeDefs[rangeID];
			const u32 rangeFrameRank = codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 25;
			const u32 rangeMip = (codecRange->frameRank7_newBlock1_grid4_blockCount20 >> 20) & 0xF;
			const SequenceBlockRange_s* srcBlockRange = &traceContext->sequenceContext.blockRanges[rangeID];
			
			hlsl::BlockRange* dstBlockRange = &blockRanges[rangeRank];
			dstBlockRange->macroGridOffset = srcBlockRange->macroGridCoords - macroGridCoords[rangeMip];
			dstBlockRange->isNewBlock = (frameRank > 0 && (frameRank - rangeFrameRank) == 0) ? 1 : srcBlockRange->isNewBlock;
			dstBlockRange->firstThreadID = firstThreadID;
			dstBlockRange->blockCount = srcBlockRange->blockCount;
			dstBlockRange->blockPosOffset = srcBlockRange->blockPosOffset;

			const u32 blockCount = srcBlockRange->blockCount;
			const u32 groupCount = HLSL_GROUP_COUNT( blockCount, HLSL_BLOCK_THREAD_GROUP_SIZE );

			for ( u32 groupRank = 0; groupRank < groupCount; ++groupRank )
				blockGroups[frameGroupCount + groupRank] = rangeRank;

			firstThreadID += groupCount * HLSL_BLOCK_THREAD_GROUP_SIZE;
			frameGroupCount += groupCount;
		}

		traceContext->frameState.groupCount = frameGroupCount;

		CodecFrameDesc_s* frameDesc = &sequence->frameDescArray[frameRank];
		traceContext->frameState.origin = frameDesc->gridOrigin;
		traceContext->frameState.bufferID = frameID & 1;
	
		GPUTraceResources_s* traceRes = traceContext->res;
		const u32 frameBlockPosOffset = traceContext->sequenceContext.frameBlockPosOffsets[frameRank];
		const CodecFrameData_s* frameData = &sequence->frameDataArray[frameRank];
		GPUBuffer_Update( &traceRes->ranges[traceContext->frameState.bufferID], 0, blockRangeBuffer, frameBlockRangeCount );
		GPUBuffer_Update( &traceRes->groups[traceContext->frameState.bufferID], 0, blockGroupBuffer, frameGroupCount );
		GPUBuffer_Update( &traceRes->blockPos, frameBlockPosOffset, frameData->blockPos, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellPresences0, frameBlockPosOffset, frameData->blockCellPresences0, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellPresences1, frameBlockPosOffset, frameData->blockCellPresences1, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellEndColors, frameBlockPosOffset, frameData->blockCellEndColors, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellColorIndices0, frameBlockPosOffset, frameData->blockCellColorIndices0, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellColorIndices1, frameBlockPosOffset, frameData->blockCellColorIndices1, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellColorIndices2, frameBlockPosOffset, frameData->blockCellColorIndices2, frameBlockCount );
		GPUBuffer_Update( &traceRes->blockCellColorIndices3, frameBlockPosOffset, frameData->blockCellColorIndices3, frameBlockCount );
	}

	traceContext->frameState.resetJitter = traceContext->frameState.frameID + 1 != frameID;

	traceContext->frameState.frameRank = targetFrameRank;
	traceContext->frameState.frameID = frameID;
	traceContext->frameState.frameChanged = true;
}

END_V6_NAMESPACE
