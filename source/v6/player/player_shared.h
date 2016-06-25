/*V6*/

#ifndef __V6_HLSL_PLAYER_SHARED_H__
#define __V6_HLSL_PLAYER_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_SURFACE_SLOT							0
#define HLSL_LCOLOR_SLOT							1
#define HLSL_RCOLOR_SLOT							2

CBUFFER( CBBasic, 0 )
{
	row_major	matrix	c_basicObjectToView;
	row_major	matrix	c_basicViewToProj;
};

CBUFFER( CBCompose, 2 )
{
	uint				c_composeFrameWidth;
	uint3				c_composeunused;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_PLAYER_SHARED_H__