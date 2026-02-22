/*V6*/

#pragma once

#ifndef __V6_GRAPHIC_TRACE_H__
#define __V6_GRAPHIC_TRACE_H__

#include <v6/codec/codec.h>
#include <v6/core/vec3.h>
#include <v6/graphic/gpu.h>

#define TRACE_SEQUENCE_BUFFER_COUNT		3
#define TRACE_FRAME_BUFFER_COUNT		3
#define TRACE_CUT_FRAME_MAX_COUNT		32

BEGIN_V6_NAMESPACE

struct TraceResource_s;
struct View_s;

struct TraceDesc_s
{
	u32						screenWidth;
	u32						screenHeight;
	bool					stereo;
};

struct TraceResource_s
{
	TraceDesc_s				desc;

	GPUConstantBuffer_s		cbCull;
	GPUConstantBuffer_s		cbProject;
	GPUConstantBuffer_s		cbTrace;
	GPUConstantBuffer_s		cbTSAA;
	GPUConstantBuffer_s		cbPostProcess;

	GPUBuffer_s				blockPos[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellPresences0[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellPresences1[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellEndColors[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellColorIndices0[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellColorIndices1[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellColorIndices2[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				blockCellColorIndices3[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				gridMacroOffsets[TRACE_SEQUENCE_BUFFER_COUNT];
	GPUBuffer_s				groups[TRACE_FRAME_BUFFER_COUNT];
	GPUBuffer_s				ranges[TRACE_FRAME_BUFFER_COUNT];

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
	const VideoSequence_s*	sequence;
	float					fadeToBlack;
	float					outOfRange;
	u32						groupCount;
	u32						sequenceID;
	u32						frameID;
	u32						frameRank;
	u32						frameCount;
	u32						perFrameBufferID;
	u32						perSequenceBufferID;
	u32						jitterID;
	u32						prevHistoryBufferID;
	u32						curHistoryBufferID;
	u32						frameUnchangedCount;
	bool					resetJitter;
};

struct TraceSequenceState_s
{
	u32 sequenceID;
	u32 sequenceBlockCount;
	u32 sequenceBlockOffset;
	u32 frameRank;
	u32 frameBlockOffset;
};

struct TraceContext_s
{
	CodecStreamDesc_s		streamDesc;
	CodecStreamData_s		streamData;
	TraceFrameState_s		frameState;
	TraceSequenceState_s	sequenceStates[TRACE_SEQUENCE_BUFFER_COUNT];
	TraceResource_s*		res;
	u32						presentRate;
	u32						cutFrames[TRACE_CUT_FRAME_MAX_COUNT];
	u32						cutFrameCount;
};

void	TraceResource_Create( TraceResource_s* res, const TraceDesc_s* traceDesc );
void	TraceResource_Release( TraceResource_s* res );

void	TraceContext_Create( TraceContext_s* traceContext, TraceResource_s* res, const CodecStreamDesc_s* streamDesc, const CodecStreamData_s* streamData, u32 presentRate );
void	TraceContext_DrawFrame( TraceContext_s* traceContext, GPURenderTargetSet_s* renderTargetSet, const View_s* views, const TraceOptions_s* options, float outOfRange );
void	TraceContext_GetFrameBasis( TraceContext_s* traceContext, Vec3* origin, float* yaw );
void	TraceContext_Release( TraceContext_s* traceContext );
void	TraceContext_UpdateFrame( TraceContext_s* traceContext, const VideoSequence_s* sequence, u32 sequenceID, u32 sequenceFrameOffset, u32 sequenceFrameRank, IStack* stack );
bool	TraceContext_StreamSequence( TraceContext_s* traceContext, const VideoSequence_s* sequence, u32 sequenceID, bool loading = false );

END_V6_NAMESPACE

#endif // __V6_GRAPHIC_TRACE_H__
