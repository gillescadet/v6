/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_TRACE_H__
#define __V6_GRAPHIC_TRACE_H__

#include <v6/codec/codec.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

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

struct GPUTraceResources_s
{
	GPUConstantBuffer_s		cbCull;
	GPUConstantBuffer_s		cbTrace;
	GPUConstantBuffer_s		cbBlend;
	GPUConstantBuffer_s		cbFilter;

	GPUBuffer_s				blockPos;
	GPUBuffer_s				blockData;
	GPUBuffer_s				ranges[2];
	GPUBuffer_s				groups[2];

	GPUBuffer_s				traceCell;
	GPUBuffer_s				traceIndirectArgs;
	
	GPUBuffer_s				cellItems;
	GPUBuffer_s				cellItemCounters;

	GPUTexture2D_s			colors;
	GPUTexture2D_s			histories[2];
	GPUBuffer_s				displacements;

	ID3D11SamplerState*		bilinearSamplerState;

	GPUBuffer_s				cullStats;
	GPUBuffer_s				traceStats;
	GPUBuffer_s				traceCellStats;

	GPUCompute_s			computeCull[2][CODEC_BUCKET_COUNT];
	GPUCompute_s			computeTraceInit;
	GPUCompute_s			computeTrace[2][CODEC_BUCKET_COUNT];
	GPUCompute_s			computeBlend[2];
	GPUCompute_s			computeFilter;
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
};

struct TraceFrameState_s
{
	Mat4x4					prevWorldToProjs[2];
	Vec3					origin;
	Vec3					basis[3];
	Vec2					jitter;
	u32						blockRangeCounts[CODEC_BUCKET_COUNT];
	u32						groupCounts[CODEC_BUCKET_COUNT];
	u32						sequenceID;
	u32						frameID;
	u32						frameRank;
	u32						bufferID;
	u32						jitterID;
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

void TraceContext_Create( TraceContext_s* traceContext, const TraceDesc_s* traceDesc, const VideoStream_s* stream );
void TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const View_s* views, const TraceOptions_s* options, IStack* stack );
void TraceContext_GetFrameBasis( TraceContext_s* traceContext, Vec3* right, Vec3* up, Vec3* forward );
void TraceContext_Release( TraceContext_s* traceContext );
void TraceContext_UpdateFrame( TraceContext_s* traceContext, u32 frameID, IStack* stack );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_TRACE_H__
