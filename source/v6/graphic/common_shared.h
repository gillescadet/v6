/*V6*/

#ifndef __V6_HLSL_COMMON_SHARED_H__
#define __V6_HLSL_COMMON_SHARED_H__

#include "../core/cpp_hlsl.h"

BEGIN_V6_HLSL_NAMESPACE

#define HLSL_GROUP_COUNT( C, S )					(((C) + (S) - 1)) / (S)

#define	HLSL_MIP_MAX_COUNT							16
#define HLSL_BUCKET_COUNT							5
#define HLSL_CELL_SUPER_SAMPLING_WIDTH				1

#define REGISTER_SRV( SLOT )						register( t ## SLOT )
#define REGISTER_UAV( SLOT )						register( u ## SLOT )

END_V6_HLSL_NAMESPACE

#endif // __V6_HLSL_COMMON_SHARED_H__