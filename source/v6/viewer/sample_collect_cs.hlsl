#define HLSL
#include "common_shared.h"
#include "sample_pack.hlsli"

static const float3 lookAts[6] = 
{
	float3( 1.0f,  0.0f,  0.0f ),
	float3( -1.0f , 0.0f, 0.0f ),
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f, -1.0f,  0.0f ), 
	float3( 0.0f,  0.0f,  1.0f ), 
	float3( 0.0f,  0.0f, -1.0f )    
};

static const float3 ups[6] =
{
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f , 1.0f, 0.0f ),
	float3( 0.0f,  0.0f, -1.0f ),
	float3( 0.0f,  0.0f,  1.0f ),
	float3( 0.0f,  1.0f,  0.0f ),
	float3( 0.0f,  1.0f,  0.0f )
};

Texture2DArray< float4 > colors					: register( HLSL_COLOR_SRV );
Texture2DArray< float > depths					: register( HLSL_DEPTH_SRV );

RWStructuredBuffer< Sample > collectedSamples	: register( HLSL_SAMPLE_UAV );
RWBuffer< uint > sampleIndirectArgs				: register( HLSL_SAMPLE_INDIRECT_ARGS_UAV );

uint GetMip( float p )
{
	return 
		dot( step( c_sampleMipBoundariesA, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) + 
		dot( step( c_sampleMipBoundariesB, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) +
		dot( step( c_sampleMipBoundariesC, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) +
		dot( step( c_sampleMipBoundariesD, p ), float4( 1.0, 1.0, 1.0, 1.0 ) );
}

[ numthreads( 16, 16, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{	
	{
		const int4 coords = int4( DTid.x, DTid.y, DTid.z, 0 );
		const float3 cubeColor = colors.Load( coords ).rgb;
		const float cubeDepth = 1.0f / ( mad ( depths.Load( coords ), c_sampleDepthLinearScale, c_sampleDepthLinearBias ) );
	
		const float3 lookAt = lookAts[DTid.z];
		const float3 up = ups[DTid.z];
		const float3 right = cross( lookAt, up );

		const float2 scale = (DTid.xy + 0.5) * c_sampleInvCubeSize * 2.0 - 1.0;
		const float3 dir = lookAt + right * scale.x - up * scale.y;
		const float3 pos = (dir * cubeDepth) + c_sampleOffset;
		const uint mip = max( GetMip( abs( pos.x ) ), max( GetMip( abs( pos.y ) ), GetMip( abs( pos.z ) ) ) );

		if ( mip < HLSL_MIP_MAX_COUNT )
		{
			const uint3 coords = int3( pos * (c_sampleInvGridScales[mip].x * HLSL_GRID_HALF_WIDTH) ) + HLSL_GRID_HALF_WIDTH;
			const uint3 color = uint3( cubeColor.rgb * 255.0f + 0.5f );

#if HLSL_DEBUG_COLLECT == 1
			if ( any( coords < 0 ) || any( coords >= HLSL_GRID_WIDTH) )
				InterlockedAdd( sample_error, 1 );
#endif // #if HLSL_DEBUG_COLLECT == 1

			uint sampleID;
			InterlockedAdd( sample_count, 1, sampleID );
			Sample_Pack( collectedSamples[sampleID], coords, mip, color );

			uint sampleCount = sampleID+1;
			InterlockedMax( sample_groupCountX, GROUP_COUNT( sampleCount, HLSL_SAMPLE_THREAD_GROUP_SIZE ) );
		}
		else
		{
#if HLSL_DEBUG_COLLECT == 1
			InterlockedAdd( sample_out, 1 );
#endif // #if HLSL_DEBUG_COLLECT == 1
		}
	}

	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		sample_groupCountY = 1;
		sample_groupCountZ = 1;
	}
}
