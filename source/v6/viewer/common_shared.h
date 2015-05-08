/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

CBUFFER( CBView, 0 )
{
	row_major	matrix objectToView;
	row_major	matrix viewToProj;
	float		zFar;
	float		depthLinearScale;
	float		depthLinearBias;
	int			_pad1;
	uint		frameWidth;
	uint		frameHeight;
	int			_pad2;
	int			_pad3;
};

#define HLSL_FIRST_SLOT		0
#define HLSL_COLOR_SLOT		0
#define HLSL_DEPTH_SLOT		1

#define CONCAT( X, Y )		X ## Y

#define HLSL_COLOR_SRV		CONCAT( t, HLSL_COLOR_SLOT )
#define HLSL_DEPTH_SRV		CONCAT( t, HLSL_DEPTH_SLOT )

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__