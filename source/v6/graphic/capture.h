/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_CAPTURE_H__
#define __V6_GRAPHIC_CAPTURE_H__

#include <v6/core/mat3x3.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

struct GPUCaptureResources_s;

struct CaptureDesc_s
{
	Mat3x3						appWorldToV6World;
	u32							gridMacroShift;
	float						gridScaleMin;
	float						gridScaleMax;
	float						depthLinearScale;
	float						depthLinearBias;
	bool						logReadBack;
};

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

struct CaptureContext_s
{
	CaptureDesc_s				desc;
	GPUCaptureResources_s*		res;
	u32							resSampleCount;
	u32							resNodeCount;
	u32							resLeafCount;
	u32							resBlockPosCount;
	u32							resBlockDataCount;
};

// any thread
void	CaptureContext_Create( CaptureContext_s* captureContext, const CaptureDesc_s* desc );
void	CaptureContext_Release( CaptureContext_s* captureContext );

// render thread
u32		CaptureContext_AddSamplesFromCubeFace( CaptureContext_s* captureContext, const Vec3* origin, const Vec3* samplePos, u32 faceID, void* colorView, void* depthView );
void	CaptureContext_Begin( CaptureContext_s* captureContext );
void	CaptureContext_End( CaptureContext_s* captureContext );
void	CaptureContext_MapBlocksForRead( CaptureContext_s* captureContext, u32* blockCounts, void** blockPos, void** blockData );
void	CaptureContext_UnmapBlocksForRead( CaptureContext_s* captureContext );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_CAPTURE_H__
