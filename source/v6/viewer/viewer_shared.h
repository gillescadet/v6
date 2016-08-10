/*V6*/

#ifndef __V6_HLSL_VIEWER_SHARED_H__
#define __V6_HLSL_VIEWER_SHARED_H__

#include "../graphic/common_shared.h"
#include "../graphic/trace_shared.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_TRILINEAR_SLOT							0

#define HLSL_SURFACE_SLOT							0
#define HLSL_LCOLOR_SLOT							1
#define HLSL_RCOLOR_SLOT							2
#define HLSL_DEPTH_SLOT								3

#define HLSL_GENERIC_ALBEDO_SLOT					4
#define HLSL_GENERIC_ALPHA_SLOT						5

CBUFFER( CBBasic, 0 )
{
	row_major	matrix	c_basicObjectToView;
	row_major	matrix	c_basicViewToProj;
	row_major	matrix	c_basicObjectToProj;
};

CBUFFER( CBGeneric, 1 )
{
	row_major	matrix	c_genericObjectToWorld;
	row_major	matrix	c_genericWorldToView;
	row_major	matrix	c_genericViewToProj;
	
	float3				c_genericDiffuse;
	int					c_genericPad0;

	int					c_genericUseAlbedo;
	int					c_genericUseAlpha;
	int					c_genericPad1;
	int					c_genericPad2;
	
};

CBUFFER( CBCompose, 2 )
{
	uint				c_composeFrameWidth;
	uint3				c_composeunused;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_VIEWER_SHARED_H__
