#include "block_render.hlsli"
#include "block_cell.hlsli"

PixelInput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID  )
{
	PixelInput o = (PixelInput)0;
	BlockCell blockCell;
	
	if ( !PackedColor_Unpack( instanceID, blockCell ) )
	{	
		o.position = float4( -2.0, -2.0, -2.0, 1.0 );
	}
	else
	{
		if ( c_blockShowVoxel )
		{
#if 1
			uint occupancyBit = 0;
			uint3 boxMin = uint3( 2, 2, 2 );
			uint3 boxMax = uint3( 0, 0, 0 );
			for ( uint z = 0; z < 3; ++z )
			{
				for ( uint y = 0; y < 3; ++y )
				{
					for ( uint x = 0; x < 3; ++x, ++occupancyBit )
					{
						if ( (blockCell.occupancy & (1 << occupancyBit)) != 0 )
						{
							const uint3 p = uint3( x, y, z );
							boxMin = min( boxMin, p );
							boxMax = max( boxMax, p );
						}
					}
				}
			}
#else
			uint3 boxMin = uint3( 0, 0, 0 );
			uint3 boxMax = uint3( 2, 2, 2 );
#endif

			const float scale = 2.0f * blockCell.halfCellSize / 3.0f; 
			const float3 pointMin = boxMin * scale;
			const float3 pointMax = (boxMax + 1.0f) * scale;
			const float3 delta = pointMax - pointMin;

			blockCell.posOS -= blockCell.halfCellSize;
			blockCell.posOS.x += pointMin.x + (((vertexID & 1) != 0) ? delta.x : 0.0f);
			blockCell.posOS.y += pointMin.y + (((vertexID & 2) != 0) ? delta.y : 0.0f);
			blockCell.posOS.z += pointMin.z + (((vertexID & 4) != 0) ? delta.z : 0.0f);
		}

		const float4 posVS = mul( c_blockObjectToView, float4( blockCell.posOS, 1.0 ) );
		const float4 posCS = mul( c_blockViewToProj, posVS );

		const float normalizationRatio = 1.0 / 255.0;
		o.color.rgb = float3( (blockCell.color >> 24) & 0xFF, (blockCell.color >> 16) & 0xFF, (blockCell.color >> 8) & 0xFF ) * normalizationRatio;
		o.color.a = 0xFF;
		o.position = posCS;
		o.uv = mad( posCS.xy / posCS.w, 0.5f, 0.5f ) * c_blockFrameSize;
		
		if ( c_blockShowMip )
		{
			o.color.r = (blockCell.mip+1) & 1 ? 1.0f : 0.0f;
			o.color.g = (blockCell.mip+1) & 2 ? 1.0f : 0.0f;
			o.color.b = (blockCell.mip+1) & 4 ? 1.0f : 0.0f;
		}

		if ( c_blockShowOverdraw )
			o.color.rgb = float3( 0.25f, 0.25f, 0.25f );
	}
	
	return o;
}