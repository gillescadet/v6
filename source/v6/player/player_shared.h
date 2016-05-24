/*V6*/

#ifndef __V6_HLSL_PLAYER_SHARED_H__
#define __V6_HLSL_PLAYER_SHARED_H__

#include "../graphic/common_shared.h"

BEGIN_V6_HLSL_NAMESPACE

CBUFFER( CBBasic, 0 )
{
	row_major	matrix	c_basicObjectToView;
	row_major	matrix	c_basicViewToProj;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_PLAYER_SHARED_H__