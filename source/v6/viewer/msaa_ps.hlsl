#define HLSL

#include "common_shared.h"

RWStructuredBuffer< uint > pixelSamplePositions : register( HLSL_PIXEL_SAMPLE_POSITION_UAV );

struct PixelInput
{
					float4 position : SV_POSITION;
	nointerpolation uint2 uv		: UV;
};

float4 main( PixelInput i ) : SV_TARGET
{
	uint sampleID;
	InterlockedAdd( pixelSamplePositions[0], 1, sampleID );
	pixelSamplePositions[sampleID+1] = i.uv.y << 16 | i.uv.x;
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}