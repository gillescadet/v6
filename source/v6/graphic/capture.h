/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_CAPTURE_H__
#define __V6_GRAPHIC_CAPTURE_H__

#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

struct CaptureDesc_s
{
	Vec3	origin;
	Vec3	samplePos;
	u32		gridMacroShift;
	float	gridScaleMin;
	float	gridScaleMax;
	float	depthLinearScale;
	float	depthLinearBias;
};

struct GPUCaptureContext_s
{
	GPUConstantBuffer_s		cb;
	GPUColorRenderTarget_s	color;
	GPUDepthRenderTarget_s	depth;
	GPUCompute_s			compute;
	u32						cbSlot;
	u32						colorSlot;
	u32						depthSlot;
};

struct GPUSampleContext_s
{
	GPUBuffer_s				samples;
	GPUBuffer_s				indirectArgs;
	u32						sampleSlot;
	u32						indirectArgSlot;
};

void GPUCaptureContext_Create( GPUCaptureContext_s* captureContext, u32 size, u32 colorSlot, u32 depthSlot );
void GPUCaptureContext_Release( GPUCaptureContext_s* captureContext );

void GPUSampleContext_Create( GPUSampleContext_s* sampleContext, u32 sampleCount, u32 sampleSlot, u32 indirectArgSlot );
void GPUSampleContext_Release( GPUSampleContext_s* sampleContext );

void Capture_Collect( const CaptureDesc_s* captureDesc, GPUCaptureContext_s* captureContext, GPUSampleContext_s* sampleContext, u32 faceID );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_CAPTURE_H__
