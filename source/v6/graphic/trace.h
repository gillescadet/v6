/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_TRACE_H__
#define __V6_GRAPHIC_TRACE_H__

#include <v6/codec/codec.h>
#include <v6/graphic/gpu.h>

BEGIN_V6_NAMESPACE

struct View_s;

struct GPUSequenceResources_s
{
	GPUBuffer_s				blockPos;
	GPUBuffer_s				blockData;
	GPUBuffer_s				ranges[2];
	GPUBuffer_s				groups[2];
};

struct GPUSequenceFrameData_s
{
	GPUSequenceResources_s* res;
	Vec3					origin;
	u32						blockRangeCounts[CODEC_BUCKET_COUNT];
	u32						groupCounts[CODEC_BUCKET_COUNT];
	u32						bufferID;
};

struct SequenceBlockRange_s
{
	Vec3i					macroGridCoords;
	u32						blockCount;
	u32						blockPosOffset;
	u32						blockDataOffset;
};

struct SequenceContext_s
{	
	GPUSequenceResources_s*	res;
	CodecRange_s*			rangeDefs[CODEC_BUCKET_COUNT];
	SequenceBlockRange_s	blockRanges[CODEC_BUCKET_COUNT][CODEC_RANGE_MAX_COUNT];
	u32						frameBlockPosOffsets[CODEC_FRAME_MAX_COUNT];
	u32						frameBlockDataOffsets[CODEC_FRAME_MAX_COUNT];
	u32						blockMaxCount;
};

struct TraceDesc_s
{
	u32						screenWidth;
	u32						screenHeight;
	u32						gridMacroShift;
	float					gridScaleMin;
	float					gridScaleMax;
	u32						blockMaxCount;
	bool					stereo;
};

struct TraceOption_s
{
	bool					logReadBack;
	bool					showHistory;
	bool					showMip;
	bool					showBucket;
	bool					showOverdraw;
	bool					randomBackground;
};

struct GPUTraceResources_s
{
	GPUConstantBuffer_s		cbCull;
	GPUConstantBuffer_s		cbBlock;
	GPUConstantBuffer_s		cbPixel;

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

struct TraceContext_s
{
	TraceDesc_s				desc;
	GPUTraceResources_s*	res;
	u32						resPassedBlockCount;
	u32						resCellItemCount;
};

void SequenceContext_CreateFromData( SequenceContext_s* sequenceContext, const Sequence_s* sequence );
void SequenceContext_Release( SequenceContext_s* sequenceContext );
void SequenceContext_UpdateFrameData( SequenceContext_s* sequenceContext, GPUSequenceFrameData_s* sequenceFrameData, u32 frameID, const Sequence_s* sequence, IStack* stack );

void TraceContext_Create( TraceContext_s* traceContext, const TraceDesc_s* traceDesc );
void TraceContext_Draw( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, GPUSequenceFrameData_s* sequenceFrameData, const View_s* views, const TraceOption_s* options );
void TraceContext_Release( TraceContext_s* traceContext );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_TRACE_H__
