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
	u32						blockDataOffset;
};

struct SequenceContext_s
{
	CodecRange_s*			rangeDefs[CODEC_BUCKET_COUNT];
	SequenceBlockRange_s	blockRanges[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	u32						frameBlockPosOffsets[CODEC_FRAME_MAX_COUNT];
	u32						frameBlockDataOffsets[CODEC_FRAME_MAX_COUNT];
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
	bool					showMip;
	bool					showBucket;
	bool					showOverdraw;
	bool					randomBackground;
	bool					noTSAA;
	bool					noSharpenFilter;
};

struct TraceFrameState_s
{
	Mat4x4					prevWorldToProjs[2];
	Vec3					origin;
	Vec2					jitter;
	u32						blockRangeCounts[CODEC_BUCKET_COUNT];
	u32						groupCounts[CODEC_BUCKET_COUNT];
	u32						sequenceID;
	u32						frameID;
	u32						frameRank;
	u32						bufferID;
	u32						jitterID;
	u32						prevHistoryBufferID;
	u32						curHistoryBufferID;
	bool					resetJitter;
};

struct TraceContext_s
{
	TraceDesc_s				desc;
	const VideoStream_s*	stream;
	GPUTraceResources_s*	res;
	SequenceContext_s		sequenceContext;
	TraceFrameState_s		frameState;
	u32						resPassedBlockCount;
	u32						resCellItemCount;
};

void	TraceContext_Create( TraceContext_s* traceContext, const TraceDesc_s* traceDesc, const VideoStream_s* stream );
void	TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const View_s* views, const TraceOptions_s* options, IStack* stack );
float	TraceContext_GetFrameYaw( TraceContext_s* traceContext );
void	TraceContext_Release( TraceContext_s* traceContext );
void	TraceContext_UpdateFrame( TraceContext_s* traceContext, u32 frameID, IStack* stack );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_TRACE_H__
