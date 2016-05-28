/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_TRACE_H__
#define __V6_GRAPHIC_TRACE_H__

#include <v6/codec/codec.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

struct View_s;

struct GPUTraceResources_s
{
	GPUConstantBuffer_s		cbCull;
	GPUConstantBuffer_s		cbBlock;
	GPUConstantBuffer_s		cbPixel;

	GPUBuffer_s				blockPos;
	GPUBuffer_s				blockData;
	GPUBuffer_s				ranges[2];
	GPUBuffer_s				groups[2];

	GPUBuffer_s				traceCell;
	GPUBuffer_s				traceIndirectArgs;
	
	GPUBuffer_s				cellItems;
	GPUBuffer_s				cellItemCounters;

	GPUTexture2D_s			colors;

	GPUBuffer_s				cullStats;
	GPUBuffer_s				traceStats;

	GPUCompute_s			computeCull[2][CODEC_BUCKET_COUNT];
	GPUCompute_s			computeTraceInit;
	GPUCompute_s			computeTrace[2][CODEC_BUCKET_COUNT];
	GPUCompute_s			computeBlend[2];
};

struct SequenceBlockRange_s
{
	Vec3i					macroGridCoords;
	u32						blockCount;
	u32						blockPosOffset;
	u32						blockDataOffset;
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
};

struct TraceContext_s
{
	TraceDesc_s				desc;
	const Sequence_s*		sequence;
	GPUTraceResources_s*	res;
	CodecRange_s*			rangeDefs[CODEC_BUCKET_COUNT];
	SequenceBlockRange_s	blockRanges[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	u32						frameBlockPosOffsets[CODEC_FRAME_MAX_COUNT];
	u32						frameBlockDataOffsets[CODEC_FRAME_MAX_COUNT];
	u32						resPassedBlockCount;
	u32						resCellItemCount;
};

struct TraceFrameData_s
{
	Vec3					origin;
	u32						blockRangeCounts[CODEC_BUCKET_COUNT];
	u32						groupCounts[CODEC_BUCKET_COUNT];
	u32						bufferID;
};

void TraceContext_Create( TraceContext_s* traceContext, const TraceDesc_s* traceDesc, const Sequence_s* sequence );
void TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const TraceFrameData_s* frameData, const View_s* views, const TraceOptions_s* options );
void TraceContext_Release( TraceContext_s* traceContext );
void TraceContext_UpdateFrame( TraceContext_s* traceContext, TraceFrameData_s* frameData, u32 frameID, IStack* stack );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_TRACE_H__
