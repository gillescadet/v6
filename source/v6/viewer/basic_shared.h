/*V6*/

#ifndef __V6_HLSL_BASIC_SHARED_H__
#define __V6_HLSL_BASIC_SHARED_H__

#include "cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

CBUFFER( CBBasicView, 0 )
{
	row_major matrix worldToView;
	row_major matrix viewToProj;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_BASIC_SHARED_H__