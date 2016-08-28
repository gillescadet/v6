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
	u32							sampleCount;;
	u32							gridMacroShift;
	float						gridScaleMin;
	float						gridScaleMax;
	float						depthLinearScale;
	float						depthLinearBias;
	bool						logReadBack;
};

struct CaptureContext_s
{
	static const u32			SAMPLE_MAX_COUNT = 32;

	CaptureDesc_s				desc;
	Vec3						sampleOffsets[SAMPLE_MAX_COUNT];
	GPUCaptureResources_s*		res;
	struct
	{
		Vec3					origin;
		u32						prevSampleLevel;
	}							frameState;
	u32							resSampleCount;
	u32							resNodeCount;
	u32							resLeafCount;
	u32							resBlockPosCount;
	u32							resBlockDataCount;
};

// any thread
void	CaptureContext_Create( CaptureContext_s* captureContext, const CaptureDesc_s* desc );
Vec3	CaptureContext_GetSampleOffset( CaptureContext_s* captureContext, u32 sampleID );
void	CaptureContext_Release( CaptureContext_s* captureContext );

// render thread
u32		CaptureContext_AddSamplesFromCubeFace( CaptureContext_s* captureContext, const Vec3* samplePos, const Vec3 basis[3], void* colorView, void* depthView );
void	CaptureContext_Begin( CaptureContext_s* captureContext, const Vec3* origin );
void	CaptureContext_End( CaptureContext_s* captureContext );
void	CaptureContext_MapBlocksForRead( CaptureContext_s* captureContext, u32* blockCounts, void** blockPos, void** blockData );
void	CaptureContext_UnmapBlocksForRead( CaptureContext_s* captureContext );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_CAPTURE_H__
