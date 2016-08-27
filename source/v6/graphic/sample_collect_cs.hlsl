#define HLSL
#include "capture_shared.h"
#include "sample_pack.hlsli"

Texture2D< float4 > colors						: REGISTER_SRV( HLSL_CAPTURE_COLOR_SLOT );
Texture2D< float > depths						: REGISTER_SRV( HLSL_CAPTURE_DEPTH_SLOT );

RWStructuredBuffer< Sample > collectedSamples	: REGISTER_UAV( HLSL_SAMPLE_SLOT );
RWBuffer< uint > sampleIndirectArgs				: REGISTER_UAV( HLSL_SAMPLE_INDIRECT_ARGS_SLOT );

uint GetMip( float3 p )
{
	for ( uint mip = 0; mip < HLSL_MIP_MAX_COUNT; ++mip )
	{
		const float3 delta = p - c_sampleMipBoundaries[mip].xyz;
		if ( all( abs( delta ) < c_sampleMipBoundaries[mip].w ) )
			return mip;
	}

	return HLSL_MIP_MAX_COUNT;
}

[ numthreads( 8, 8, 1 ) ]
void main_sample_collect_cs( uint3 DTid : SV_DispatchThreadID )
{
	const uint gridHalfWidth = c_sampleGridWidth >> 1;

	{
		const uint2 pixelCoords = uint2( DTid.xy );
		const int3 coords = int3( pixelCoords.x, pixelCoords.y, 0 );
		const float3 cubeColor = colors.Load( coords ).rgb;
		const float cubeDepth = rcp( mad ( depths.Load( coords ), c_sampleDepthLinearScale, c_sampleDepthLinearBias ) );
	
		const float2 scale = mad( pixelCoords.xy + 0.5f, c_sampleInvCubeSize * 2.0f, -1.0f );
		const float3 dir = c_sampleForward.xyz + c_sampleRight.xyz * scale.x - c_sampleUp.xyz * scale.y;
		const float3 pos = mad( dir, cubeDepth, c_samplePos );
		const uint mip = GetMip( pos );

		const bool isVisible = dot( c_sampleGridOrigin - pos, c_samplePos - pos ) > 0.0f;

		if ( mip < HLSL_MIP_MAX_COUNT && isVisible )
		{
			const float3 posInMip = pos - c_sampleMipBoundaries[mip].xyz;
			const float3 cellCoords = mad( posInMip, c_sampleInvGridScales[mip].x * gridHalfWidth, gridHalfWidth );
			const int3 sampleCoords = int3( cellCoords );

			uint sampleID;
			InterlockedAdd( sample_count, 1, sampleID );
			Sample_Pack( collectedSamples[sampleID], sampleCoords, mip, uint3( mad( cubeColor.rgb, 255.0f, 0.5f ) ) );

			const uint sampleCount = sampleID+1;
			InterlockedMax( sample_groupCountX, HLSL_GROUP_COUNT( sampleCount, HLSL_SAMPLE_THREAD_GROUP_SIZE ) );

#if HLSL_DEBUG_COLLECT == 1
			if ( any( sampleCoords < 0 ) || any( sampleCoords >= (int)c_sampleGridWidth ) )
				InterlockedAdd( sample_error, 1 );
#if 0
			InterlockedOr( sample_occupancy, occupancy );
			sample_cellCoords( sample_pixelSampleID ) = asuint( cellCoords.x );
#endif
			InterlockedAdd( sample_pixelCount, 1 );
#endif // #if HLSL_DEBUG_COLLECT == 1
		}
	}
	
	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		InterlockedMax( sample_groupCountX, uint( 1 ) );
		sample_groupCountY = 1;
		sample_groupCountZ = 1;
	}
}
