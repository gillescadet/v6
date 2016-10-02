/*V6*/

#ifndef __V6_HLSL_COMMON_SHARED_H__
#define __V6_HLSL_COMMON_SHARED_H__

#include "../core/cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_FLT_MAX								3.402823466e+38F
#define HLSL_UINT_MAX								0xFFFFFFFF

#define HLSL_GROUP_COUNT( C, S )					(((C) + (S) - 1)) / (S)

#define	HLSL_MIP_MAX_COUNT							16
#define HLSL_FACE_MAX_COUNT							8
#define HLSL_GRID_MAX_COUNT							16
#define HLSL_BUCKET_COUNT							5

#define HLSL_MIP_MACRO_XYZ_BIT_COUNT				9
#define HLSL_MIP_MACRO_XYZ_BIT_MASK					((1 << HLSL_MIP_MACRO_XYZ_BIT_COUNT) - 1)
#define HLSL_ONION_MACRO_Z_BIT_COUNT				11
#define HLSL_ONION_MACRO_Z_BIT_MASK					((1 << HLSL_ONION_MACRO_Z_BIT_COUNT) - 1)

#define REGISTER_SAMPLER( SLOT )					register( s ## SLOT )
#define REGISTER_SRV( SLOT )						register( t ## SLOT )
#define REGISTER_UAV( SLOT )						register( u ## SLOT )

struct uint64
{
	uint	low;
	uint	high;
};

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_COMMON_SHARED_H__