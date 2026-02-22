/*V6*/

#ifndef __V6_HLSL_FONT_SHARED_H__
#define __V6_HLSL_FONT_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_TRILINEAR_SLOT							0

#define HLSL_FONT_CHARACTER_SLOT					0
#define HLSL_FONT_TEXTURE_SLOT						1

CBUFFER( CBFont, 0 )
{
	float4				c_fontMatRow0;
	float4				c_fontMatRow1;
	float4				c_fontMatRow2;
	float4				c_fontMatRow3;
	float4				c_fontBackgroundQuad;
	float4				c_fontBackgroundColor;
	float2				c_fontInvBitmapSize;
	float2				c_fontUnused;
};

struct Character
{
	uint	posx16_posy16;
	uint	x8_y8_w8_h8;
	uint	rgba;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_FONT_SHARED_H__
