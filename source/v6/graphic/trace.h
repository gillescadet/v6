/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_TRACE_H__
#define __V6_GRAPHIC_TRACE_H__

#include <v6/codec/codec.h>
#include <v6/core/vec3.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

struct GPUTraceResources_s;
struct View_s;

struct SequenceBlockRange_s
{
	Vec3i					macroGridCoords;
	u32						blockCount;
	u32						blockPosOffset;
	u32						isNewBlock;
};

struct SequenceContext_s
{
	SequenceBlockRange_s	blockRanges[CODEC_RANGE_MAX_COUNT];
	u32						frameBlockPosOffsets[CODEC_FRAME_MAX_COUNT];
};

struct TraceDesc_s
{
	u32						screenWidth;
	u32						screenHeight;
	bool					stereo;
};

struct TraceOptions_s
{
	bool					logReadBack;
	bool					showHistory;
	bool					showGrid;
	bool					showOverdraw;
	bool					showBlock;
	bool					noTSAA;
	bool					noSharpenFilter;
};

struct TraceFrameState_s
{
	Vec4					prevWorldToProjsX[2];
	Vec4					prevWorldToProjsY[2];
	Vec4					prevWorldToProjsW[2];
	Vec4					curWorldToProjsX[2];
	Vec4					curWorldToProjsY[2];
	Vec4					curWorldToProjsW[2];
	Vec3					origin;
	Vec2					jitter;
	float					fadeToBlack;
	u32						groupCount;
	u32						sequenceID;
	u32						frameID;
	u32						frameRank;
	u32						bufferID;
	u32						jitterID;
	u32						prevHistoryBufferID;
	u32						curHistoryBufferID;
	bool					resetJitter;
	bool					frameChanged;
};

struct TraceContext_s
{
	TraceDesc_s				desc;
	const VideoStream_s*	stream;
	GPUTraceResources_s*	res;
	SequenceContext_s		sequenceContext;
	TraceFrameState_s		frameState;
	u32						resVisibleBlockMaxCount;
	u32						resBlockPatchCountPerEye;
};

void	TraceContext_Create( TraceContext_s* traceContext, const TraceDesc_s* traceDesc, const VideoStream_s* stream );
void	TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const View_s* views, const TraceOptions_s* options, float fadeToBlack );
void	TraceContext_GetFrameBasis( TraceContext_s* traceContext, Vec3* origin, float* yaw );
void	TraceContext_Release( TraceContext_s* traceContext );
void	TraceContext_UpdateFrame( TraceContext_s* traceContext, u32 frameID, IStack* stack );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_TRACE_H__
