/*V6*/

#pragma warning( push, 3 )
#include <d3d11_1.h>
#pragma warning( pop )

#include <v6/core/common.h>

#include <v6/core/codec.h>
#include <v6/graphic/capture.h>
#include <v6/graphic/capture_shared.h>

BEGIN_V6_NAMESPACE

BEGIN_HLSL_BYTECODE( sample_collect_cs )
#include <v6/graphic/sample_collect_cs_bytecode.h>
END_HLSL_BYTECODE

extern ID3D11Device*		g_device;
extern ID3D11DeviceContext* g_deviceContext;

void GPUCaptureContext_Create( GPUCaptureContext_s* captureContext, u32 size, u32 colorSlot, u32 depthSlot )
{
	GPUConstantBuffer_Create( &captureContext->cb, sizeof( hlsl::CBSample ), "sample" );
	GPUColorRenderTarget_Create( &captureContext->color, size, size, 1, true, false, "cubeColor" );
	GPUDepthRenderTarget_Create( &captureContext->depth, size, size, 1, true, "cubeDepth" );
	GPUCompute_CreateFromSource( &captureContext->compute, hlsl::sample_collect_cs::g_main, sizeof( hlsl::sample_collect_cs::g_main ) );
	captureContext->colorSlot = colorSlot;
	captureContext->depthSlot = depthSlot;
}

void GPUCaptureContext_Release( GPUCaptureContext_s* captureContext )
{
	GPUConstantBuffer_Release( &captureContext->cb );
	GPUColorRenderTarget_Release( &captureContext->color );
	GPUDepthRenderTarget_Release( &captureContext->depth );
	GPUCompute_Release( &captureContext->compute );
}

void GPUSampleContext_Create( GPUSampleContext_s* sampleContext, u32 sampleCount, u32 sampleSlot, u32 indirectArgSlot )
{
	GPUBuffer_CreateStructured( &sampleContext->samples, sizeof( hlsl::Sample ), sampleCount, 0, "samples" );
	GPUBuffer_CreateIndirectArgs( &sampleContext->indirectArgs, sample_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "sampleIndirectArgs" );
	sampleContext->sampleSlot = sampleSlot;
	sampleContext->indirectArgSlot = indirectArgSlot;
}

void GPUSampleContext_Release( GPUSampleContext_s* sampleContext )
{
	GPUBuffer_Release( &sampleContext->samples );
	GPUBuffer_Release( &sampleContext->indirectArgs );
}

void Capture_Collect( const CaptureDesc_s* captureDesc, GPUCaptureContext_s* captureContext, GPUSampleContext_s* sampleContext, u32 faceID )
{
	GPU_BeginEvent( "Collect" );

	// Update buffers

	V6_ASSERT( captureDesc->gridMacroShift > 0 );
	const u32 gridMacroHalfWidth = 1 << (captureDesc->gridMacroShift-1);
	const u32 gridWidth = 1 << (captureDesc->gridMacroShift + 2);
	const u32 cubeWidth = gridWidth * HLSL_CELL_SUPER_SAMPLING_WIDTH;

	{
		float gridScales[CODEC_MIP_MAX_COUNT];
		Vec3 gridCenters[CODEC_MIP_MAX_COUNT];
		float gridScale = captureDesc->gridScaleMin;
		for ( u32 gridID = 0;  gridID < CODEC_MIP_MAX_COUNT; ++gridID )
		{
			gridScales[gridID] = gridScale;
			gridCenters[gridID] = Codec_ComputeGridCenter( &captureDesc->origin, gridScale, gridMacroHalfWidth );
			if ( gridScale < captureDesc->gridScaleMax )
				gridScale *= 2;
		}

		hlsl::CBSample* cbSample = (hlsl::CBSample*)GPUConstantBuffer_MapWrite( &captureContext->cb );

		cbSample->c_sampleDepthLinearScale = captureDesc->depthLinearScale; // -1.0f / ZNEAR;
		cbSample->c_sampleDepthLinearBias = captureDesc->depthLinearBias; // 1.0f / ZNEAR;
		cbSample->c_sampleGridWidth = gridWidth;
		cbSample->c_sampleInvCubeSize = 1.0f / cubeWidth;
		cbSample->c_samplePos = captureDesc->samplePos;
		cbSample->c_sampleFaceID = faceID;		
		for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleMipBoundaries[gridID] = Vec4_Make( &gridCenters[gridID], gridScales[gridID] );
		for ( u32 gridID = 0; gridID < CODEC_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleInvGridScales[gridID] = Vec4_Make( 1.0f / gridScales[gridID], 0.0f, 0.0f , 0.0f );

		GPUConstantBuffer_UnmapWrite( &captureContext->cb );
	}

	u32 values[4] = {};
	g_deviceContext->ClearUnorderedAccessViewUint( sampleContext->indirectArgs.uav, values );

	// Set
	g_deviceContext->CSSetConstantBuffers( captureContext->cbSlot, 1, &captureContext->cb.buf );
	g_deviceContext->CSSetShaderResources( captureContext->colorSlot, 1, &captureContext->color.srv );
	g_deviceContext->CSSetShaderResources( captureContext->depthSlot, 1, &captureContext->depth.srv );
	g_deviceContext->CSSetUnorderedAccessViews( sampleContext->sampleSlot, 1, &sampleContext->samples.uav, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( sampleContext->indirectArgSlot, 1, &sampleContext->indirectArgs.uav, nullptr );
	g_deviceContext->CSSetShader( captureContext->compute.m_computeShader, nullptr, 0 );

	// Dispatch
	const u32 cubeGroupCount = gridWidth >> 3;
	g_deviceContext->Dispatch( cubeGroupCount, cubeGroupCount, 1 );

	// Unset
	static const void* nulls[8] = {};
	g_deviceContext->CSSetShaderResources( captureContext->colorSlot, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetShaderResources( captureContext->depthSlot, 1, (ID3D11ShaderResourceView**)nulls );
	g_deviceContext->CSSetUnorderedAccessViews( sampleContext->sampleSlot, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	g_deviceContext->CSSetUnorderedAccessViews( sampleContext->indirectArgSlot, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	GPU_EndEvent();
}

END_V6_NAMESPACE
