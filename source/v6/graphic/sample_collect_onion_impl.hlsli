#define HLSL
#include "capture_shared.h"
#include "sample_pack.hlsli"

Texture2D< float4 > colors							: REGISTER_SRV( HLSL_CAPTURE_COLOR_SLOT );
Texture2D< float > depths							: REGISTER_SRV( HLSL_CAPTURE_DEPTH_SLOT );

RWStructuredBuffer< Sample > collectedSamples		: REGISTER_UAV( HLSL_SAMPLE_SLOT );
RWBuffer< uint > sampleIndirectArgs					: REGISTER_UAV( HLSL_SAMPLE_INDIRECT_ARGS_SLOT );

#if SAMPLE_DEBUG == 1
RWStructuredBuffer< SampleDebugOnion > sampleDebug : REGISTER_UAV( HLSL_SAMPLE_DEBUG_SLOT );
#endif // #if CAPTURE_DEBUG == 1

bool Assert( bool condition, uint id )
{
#if SAMPLE_DEBUG == 1
	if ( !condition )
	{
		uint prev;
		InterlockedOr( sampleDebug[0].assertFailedBits, 1 << id, prev );
		return prev == 0;
	}
	else
#endif // #if BLOCK_DEBUG == 1
	{
		return false;
	}
}

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint2 pixelCoords = uint2( DTid.xy );
	const int3 textureCoords = int3( pixelCoords.x, pixelCoords.y, 0 );
	const float3 cubeColor = colors.Load( textureCoords ).rgb;
	const float cubeDepth = rcp( mad ( depths.Load( textureCoords ), c_sampleOnionDepthLinearScale, c_sampleOnionDepthLinearBias ) );
	
	const float2 scale = mad( pixelCoords.xy + 0.5f, c_sampleOnionInvCubeSize * 2.0f, -1.0f );
	const float3 dir = c_sampleOnionForward.xyz + c_sampleOnionRight.xyz * scale.x - c_sampleOnionUp.xyz * scale.y;
	const float3 posFromDepthBuffer = mad( dir, cubeDepth, c_sampleOnionSampleCenterWS.xyz );

	const float3 posFromDepthBufferRS = posFromDepthBuffer - c_sampleOnionGridCenterWS.xyz;

	float3 posRS;

	if ( any( (abs( posFromDepthBufferRS ) + 1.0f) > c_sampleOnionGridMaxScale ) )
	{
		const float3 invDir = rcp( dir );
		const float3 t0 = c_sampleOnionSkyboxMinRS.xyz * invDir;
		const float3 t1 = c_sampleOnionSkyboxMaxRS.xyz * invDir;
		const float3 tMax = max( t0, t1 );
		const float tSky = min( min( tMax.x, tMax.y ), tMax.z );

		const float3 posSky = mad( dir, tSky, c_sampleOnionSampleCenterWS.xyz );
		posRS = posSky - c_sampleOnionGridCenterWS.xyz;
	}
	else
	{
		posRS = posFromDepthBufferRS;
	}

	const float3 absPosRS = abs( posRS );
		
	uint axis;
	uint sign;
	float3 posGS;
	if ( absPosRS.x > absPosRS.y && absPosRS.x > absPosRS.z )
	{
		axis = 0;
		sign = posRS.x < 0.0f ? 1 : 0;
		posGS.z = absPosRS.x;
		posGS.x = posRS.y;
		posGS.y = posRS.z;
	}
	else if ( absPosRS.y > absPosRS.z )
	{
		axis = 1;
		sign = posRS.y < 0.0f ? 1 : 0;
		posGS.z = absPosRS.y;
		posGS.x = posRS.z;
		posGS.y = posRS.x;
	}
	else
	{
		axis = 2;
		sign = posRS.z < 0.0f ? 1 : 0;
		posGS.z = absPosRS.z;
		posGS.x = posRS.x;
		posGS.y = posRS.y;
	}

	if ( posGS.z >= c_sampleOnionGridMinScale )
	{
		uint3 blockCoords;

		blockCoords.z = clamp( uint( log2( posGS.z * c_sampleOnionInvGridMinScale ) * c_sampleOnionMacroPeriodWidth ), 0, (1u << HLSL_ONION_MACROZ_BIT_COUNT)-1 );

		float3 blockPosMin, blockPosMax;
		blockPosMin.z = c_sampleOnionGridMinScale * exp2( (blockCoords.z + 0.0f) * c_sampleOnionInvMacroPeriodWidth );
		blockPosMax.z = c_sampleOnionGridMinScale * exp2( (blockCoords.z + 1.0f) * c_sampleOnionInvMacroPeriodWidth );

		blockCoords.xy = clamp( uint2( mad( posGS.xy, c_sampleOnionHalfMacroGridWidth * rcp( blockPosMax.z ), c_sampleOnionHalfMacroGridWidth ) ), 0, c_sampleOnionMacroGridWidth-1 );
		blockPosMin.xy = mad( blockCoords.xy + 0.0f, c_sampleOnionInvMacroGridWidth * 2.0f, -1.0f ) * blockPosMax.z;
		blockPosMax.xy = mad( blockCoords.xy + 1.0f, c_sampleOnionInvMacroGridWidth * 2.0f, -1.0f ) * blockPosMax.z;

		const uint3 cellCoords = clamp( uint3( (posGS - blockPosMin) * 4.0f / (blockPosMax - blockPosMin) ), 0, 3 );

#if SAMPLE_DEBUG == 1
		InterlockedMin( sampleDebug[0].minBlockCoords.x, blockCoords.x );
		InterlockedMin( sampleDebug[0].minBlockCoords.y, blockCoords.y );
		InterlockedMin( sampleDebug[0].minBlockCoords.z, blockCoords.z );

		InterlockedMax( sampleDebug[0].maxBlockCoords.x, blockCoords.x );
		InterlockedMax( sampleDebug[0].maxBlockCoords.y, blockCoords.y );
		InterlockedMax( sampleDebug[0].maxBlockCoords.z, blockCoords.z );
#endif

		uint sampleID;
		InterlockedAdd( sample_count, 1, sampleID );
		SampleOnion_Pack( collectedSamples[sampleID], axis, sign, blockCoords, cellCoords, uint3( mad( cubeColor.rgb, 255.0f, 0.5f ) ) );

		const uint sampleCount = sampleID+1;
		InterlockedMax( sample_groupCountX, HLSL_GROUP_COUNT( sampleCount, HLSL_SAMPLE_THREAD_GROUP_SIZE ) );
	}
	
	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		InterlockedMax( sample_groupCountX, uint( 1 ) );
		sample_groupCountY = 1;
		sample_groupCountZ = 1;
	}
}
