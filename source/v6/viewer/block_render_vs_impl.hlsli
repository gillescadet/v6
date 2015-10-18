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
			blockCell.posOS.x += ((vertexID & 1) == 0) ? -blockCell.halfCellSize : blockCell.halfCellSize;
			blockCell.posOS.y += ((vertexID & 2) == 0) ? -blockCell.halfCellSize : blockCell.halfCellSize;
			blockCell.posOS.z += ((vertexID & 4) == 0) ? -blockCell.halfCellSize : blockCell.halfCellSize;		
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