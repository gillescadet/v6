#include "block_render.hlsli"
#include "block_cell.hlsli"

PixelInput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID  )
{
	PixelInput o = (PixelInput)0;
	BlockCell blockCell;
	
	if ( !PackedColor_Unpack( instanceID, blockCell, c_blockShowVoxel ? vertexID : -1 ) )
	{		
		o.position = blockCell.posCS;
	}
	else
	{
		const float normalizationRatio = 1.0 / 255.0;
		o.color.rgb = float3( (blockCell.color >> 24) & 0xFF, (blockCell.color >> 16) & 0xFF, (blockCell.color >> 8) & 0xFF ) * normalizationRatio;
		o.color.a = 0xFF;	
		o.position = blockCell.posCS;
		o.uv = mad( blockCell.posCS.xy / blockCell.posCS.w, 0.5f, 0.5f ) * c_blockFrameSize;
	}
	
	return o;
}