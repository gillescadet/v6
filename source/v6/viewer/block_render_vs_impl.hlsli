#include "block_render.hlsli"
#include "block_cell.hlsli"

void ComputeBoundigBoxFromOccupancy( uint occupancy, out uint3 boxMin, out uint3 boxMax )
{
	uint occupancyBit = 0;
	boxMin = uint3( 2, 2, 2 );
	boxMax = uint3( 0, 0, 0 );

	for ( uint z = 0; z < 3; ++z )
	{
		for ( uint y = 0; y < 3; ++y )
		{
			for ( uint x = 0; x < 3; ++x, ++occupancyBit )
			{
				if ( (occupancy & (1 << occupancyBit)) != 0 )
				{
					const uint3 p = uint3( x, y, z );
					boxMin = min( boxMin, p );
					boxMax = max( boxMax, p );
				}
			}
		}
	}
}

bool ComputeSubBoundigBoxFromOccupancy( uint occupancy, uint subVoxelID, out uint3 boxMin, out uint3 boxMax )
{
	uint occupancyBit = 0;
	boxMin = uint3( 2, 2, 2 );
	boxMax = uint3( 0, 0, 0 );

	for ( uint z = 0; z < 3; ++z )
	{
		for ( uint y = 0; y < 3; ++y )
		{
			for ( uint x = 0; x < 3; ++x, ++occupancyBit )
			{
				if ( subVoxelID != occupancyBit )
					continue;

				if ( (occupancy & (1 << occupancyBit)) != 0 )
				{
					const uint3 p = uint3( x, y, z );
					boxMin = p;
					boxMax = p;
					return false;
				}

				return true;
			}
		}
	}

	return true;
}

PixelInput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID  )
{
	PixelInput o = (PixelInput)0;
	BlockCell blockCell;

	bool skip = false;
	uint subVoxelID = 0;
	if ( c_blockUseOccupancy == 2 )
	{		
		subVoxelID = instanceID % HLSL_PIXEL_SUPER_SAMPLING_WIDTH_CUBE;
		instanceID = instanceID / HLSL_PIXEL_SUPER_SAMPLING_WIDTH_CUBE;
	}

	if ( !PackedColor_Unpack( instanceID, blockCell ) )
	{	
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );
	}
	else
	{
		if ( c_blockShowVoxel )
		{
			uint3 boxMin = uint3( 0, 0, 0 );
			uint3 boxMax = uint3( 2, 2, 2 );

			if ( c_blockUseOccupancy == 1 )
				ComputeBoundigBoxFromOccupancy( blockCell.occupancy, boxMin, boxMax );
			else if ( c_blockUseOccupancy == 2 )
				skip = ComputeSubBoundigBoxFromOccupancy( blockCell.occupancy, subVoxelID, boxMin, boxMax );

			const float scale = 2.0f * blockCell.halfCellSize / 3.0f; 
			const float3 pointMin = boxMin * scale;
			const float3 pointMax = (boxMax + 1.0f) * scale;
			const float3 delta = pointMax - pointMin;

			blockCell.posWS -= blockCell.halfCellSize;
			blockCell.posWS.x += pointMin.x + (((vertexID & 1) != 0) ? delta.x : 0.0f);
			blockCell.posWS.y += pointMin.y + (((vertexID & 2) != 0) ? delta.y : 0.0f);
			blockCell.posWS.z += pointMin.z + (((vertexID & 4) != 0) ? delta.z : 0.0f);
		}

		const float4 posVS = mul( c_blockObjectToView, float4( blockCell.posWS, 1.0 ) );
		const float4 posCS = mul( c_blockViewToProj, posVS );

		const float normalizationRatio = 1.0 / 255.0;
		o.color.rgb = float3( (blockCell.color >> 24) & 0xFF, (blockCell.color >> 16) & 0xFF, (blockCell.color >> 8) & 0xFF ) * normalizationRatio;
		o.color.a = 0xFF;
		o.position = posCS;
		o.uv = mad( posCS.xy / posCS.w, 0.5f, 0.5f ) * c_blockFrameSize;
	
#if 0
		if ( subVoxelID < 9 )
		{
			o.color.r = 1.0f;
			o.color.g = 0.0f;
			o.color.b = 0.0f;
		}
		else if ( subVoxelID < 18 )
		{
			o.color.r = 0.0f;
			o.color.g = 1.0f;
			o.color.b = 0.0f;
		}
		else
		{
			o.color.r = 0.0f;
			o.color.g = 0.0f;
			o.color.b = 1.0f;
		}
#endif

#if 0
		{
			const uint hash = (subVoxelID%7)+1;
			o.color.r = hash & 1 ? 1.0f : 0.0f;
			o.color.g = hash & 2 ? 1.0f : 0.0f;
			o.color.b = hash & 4 ? 1.0f : 0.0f;
		}
#endif

#if 0
		{
			const uint hash = (instanceID%7)+1;
			o.color.r = hash & 1 ? 1.0f : 0.0f;
			o.color.g = hash & 2 ? 1.0f : 0.0f;
			o.color.b = hash & 4 ? 1.0f : 0.0f;
		}
#endif

#if 0
		if ( blockCell.occupancy == 0 )
		{
			o.color.r = 0.25f;
			o.color.g = 0.25f;
			o.color.b = 0.25f;
		}
#endif

#if 0
		{
			o.color.r = (GRID_CELL_BUCKET+1) & 1 ? 1.0f : 0.0f;
			o.color.g = (GRID_CELL_BUCKET+1) & 2 ? 1.0f : 0.0f;
			o.color.b = (GRID_CELL_BUCKET+1) & 4 ? 1.0f : 0.0f;
		}
#endif

		if ( c_blockShowMip )
		{
			o.color.r = (blockCell.mip+1) & 1 ? 1.0f : 0.0f;
			o.color.g = (blockCell.mip+1) & 2 ? 1.0f : 0.0f;
			o.color.b = (blockCell.mip+1) & 4 ? 1.0f : 0.0f;
		}

		if ( c_blockShowOverdraw )
			o.color.rgb = float3( 0.25f, 0.25f, 0.25f );
	}
	
	if ( skip )
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );

	return o;
}