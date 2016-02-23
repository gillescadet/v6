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

Texture2D< float4 > colors						: register( HLSL_LCOLOR_SRV );
Texture2D< float > depths						: register( HLSL_DEPTH_SRV );

RWStructuredBuffer< Sample > collectedSamples	: register( HLSL_SAMPLE_UAV );
RWBuffer< uint > sampleIndirectArgs				: register( HLSL_SAMPLE_INDIRECT_ARGS_UAV );

uint GetMip( float p )
{
#if HLSL_MIP_MAX_COUNT == 8
	return 
		dot( step( c_sampleMipBoundariesA, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) + 
		dot( step( c_sampleMipBoundariesB, p ), float4( 1.0, 1.0, 1.0, 1.0 ) );
#endif

#if HLSL_MIP_MAX_COUNT == 16
	return 
		dot( step( c_sampleMipBoundariesA, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) + 
		dot( step( c_sampleMipBoundariesB, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) +
		dot( step( c_sampleMipBoundariesC, p ), float4( 1.0, 1.0, 1.0, 1.0 ) ) +
		dot( step( c_sampleMipBoundariesD, p ), float4( 1.0, 1.0, 1.0, 1.0 ) );
#endif
}


struct PixelSample
{
	int3 coords;
	uint mip;
	float3 color;	
	uint occupancy;
	uint count;
};

[ numthreads( 8, 8, 1 ) ]
void main( uint3 DTid : SV_DispatchThreadID )
{	
	const float3 lookAt = lookAts[c_sampleFaceID];
	const float3 up = ups[c_sampleFaceID];
	const float3 right = cross( lookAt, up );
			
	PixelSample pixelSamples[HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ];
	uint uniquePixelSampleCount = 0;

	for ( uint j = 0; j < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++j )
	{
		for ( uint i = 0; i < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++i )
		{
			const uint2 pixelCoords = uint2( DTid.x * HLSL_CELL_SUPER_SAMPLING_WIDTH + i, DTid.y * HLSL_CELL_SUPER_SAMPLING_WIDTH + j );
			const int3 coords = int3( pixelCoords.x, pixelCoords.y, 0 );
			const float3 cubeColor = colors.Load( coords ).rgb;
			const float cubeDepth = 1.0f / ( mad ( depths.Load( coords ), c_sampleDepthLinearScale, c_sampleDepthLinearBias ) );
	
			const float2 scale = (pixelCoords.xy + 0.5f) * c_sampleInvCubeSize * 2.0f - 1.0f;
			const float3 dir = lookAt + right * scale.x - up * scale.y;
			const float3 pos = mad( dir, cubeDepth, c_sampleOffset );
			const uint mip = max( GetMip( abs( pos.x ) ), max( GetMip( abs( pos.y ) ), GetMip( abs( pos.z ) ) ) );

			if ( mip >= HLSL_MIP_MAX_COUNT )
				continue;

#if HLSL_DEBUG_COLLECT == 1
			uint sample_pixelSampleID;
			InterlockedAdd( sample_pixelSampleCount, 1, sample_pixelSampleID );
#endif // #if HLSL_DEBUG_COLLECT == 1

			const float3 cellCoords = mad( pos, c_sampleInvGridScales[mip].x * HLSL_GRID_HALF_WIDTH, HLSL_GRID_HALF_WIDTH );
			const int3 sampleCoords = int3( cellCoords );
			const int3 subCellCoords = min( int3( frac( cellCoords ) * HLSL_CELL_SUPER_SAMPLING_WIDTH ), 2 );
			const uint subCellID = subCellCoords.z * HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ + subCellCoords.y * HLSL_CELL_SUPER_SAMPLING_WIDTH + subCellCoords.x;
			const uint occupancy = 1 << subCellID;

#if HLSL_DEBUG_COLLECT == 1
			if ( any( sampleCoords < 0 ) || any( sampleCoords >= HLSL_GRID_WIDTH ) )
				InterlockedAdd( sample_error, 1 );
#if 0
			InterlockedOr( sample_occupancy, occupancy );
			sample_cellCoords( sample_pixelSampleID ) = asuint( cellCoords.x );
#endif
#endif // #if HLSL_DEBUG_COLLECT == 1

			uint uniquePixelSampleID;
			for ( uniquePixelSampleID = 0; uniquePixelSampleID < uniquePixelSampleCount; ++uniquePixelSampleID )
			{
				if ( pixelSamples[uniquePixelSampleID].mip == mip && all( pixelSamples[uniquePixelSampleID].coords == sampleCoords ) )
					break;
			}

			if ( uniquePixelSampleID < uniquePixelSampleCount )
			{
				pixelSamples[uniquePixelSampleID].color += cubeColor.rgb;
				pixelSamples[uniquePixelSampleID].occupancy |= occupancy;
				pixelSamples[uniquePixelSampleID].count += 1;
			}
			else
			{
				pixelSamples[uniquePixelSampleID].coords = sampleCoords;
				pixelSamples[uniquePixelSampleID].mip = mip;
				pixelSamples[uniquePixelSampleID].color = cubeColor.rgb;
				pixelSamples[uniquePixelSampleID].occupancy = occupancy;
				pixelSamples[uniquePixelSampleID].count = 1;

				++uniquePixelSampleCount;
			}
		}
	}

#if HLSL_DEBUG_COLLECT == 1
	if ( uniquePixelSampleCount == 0)
		InterlockedAdd( sample_out, 1 );
	else
		InterlockedAdd( sample_pixelCount, 1 );
#endif // #if HLSL_DEBUG_COLLECT == 1

	for ( uint uniquePixelSampleID = 0; uniquePixelSampleID < uniquePixelSampleCount; ++uniquePixelSampleID )
	{
		PixelSample uniquePixelSample = pixelSamples[uniquePixelSampleID];

		uint sampleID;
		InterlockedAdd( sample_count, 1, sampleID );
		Sample_Pack( collectedSamples[sampleID], uniquePixelSample.coords, uniquePixelSample.mip, uint3( mad( uniquePixelSample.color / uniquePixelSample.count, 255.0f, 0.5f ) ), uniquePixelSample.occupancy );

//#if HLSL_DEBUG_COLLECT == 1
//		InterlockedAdd( sample_pixelSampleCount, uniquePixelSample.count );
//#endif // #if HLSL_DEBUG_COLLECT == 1

		uint sampleCount = sampleID+1;
		InterlockedMax( sample_groupCountX, GROUP_COUNT( sampleCount, HLSL_SAMPLE_THREAD_GROUP_SIZE ) );
	}
	
	if ( DTid.x == 0 && DTid.y == 0 && DTid.z == 0 )
	{
		InterlockedMax( sample_groupCountX, uint( 1 ) );
		sample_groupCountY = 1;
		sample_groupCountZ = 1;
	}
}
