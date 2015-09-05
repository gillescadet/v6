/*V6*/

#ifndef __V6_HLSL_SAMPLE_PACK_H__
#define __V6_HLSL_SAMPLE_PACK_H__

#define HLSL

#include "common_shared.h"

void Sample_Pack( out Sample s, uint3 coords, uint mip, uint3 color )
{
	s.row0 = (coords.x << 20) | (coords.y << 8) | color.r;
	s.row1 = (coords.z << 20) | (color.g << 12) | (color.b << 4) | mip;
}

void Sample_Unpack( Sample s, out uint3 coords, out uint mip, out uint3 color  )
{
	coords.x = s.row0 >> 20;
	coords.y = (s.row0 >> 8) & 0xFFF;
	coords.z = s.row1 >> 20;
	
	color.r = s.row0 & 0xFF;
	color.g = (s.row1 >> 12) & 0xFF;
	color.b = (s.row1 >> 4) & 0xFF;
	
	mip = s.row1 & 0xF;
}

uint Sample_UnpackMip( Sample s )
{
	return s.row1 & 0xF;
}

#endif // __V6_HLSL_SAMPLE_PACK_H__
