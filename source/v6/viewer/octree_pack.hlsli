/*V6*/

#ifndef __V6_HLSL_OCTREE_PACK_H__
#define __V6_HLSL_OCTREE_PACK_H__

#define HLSL

#include "common_shared.h"

Buffer< uint > firstChildOffsets				: register( HLSL_FIRST_CHILD_OFFSET_SRV );
StructuredBuffer< OctreeLeaf > octreeLeaves		: register( HLSL_OCTREE_LEAF_SRV );
RWBuffer< uint > sampleIndirectArgs 			: register( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_UAV );
RWBuffer< uint > packedColors					: register( HLSL_GRID_PACKEDCOLOR_UAV );

#endif // __V6_HLSL_OCTREE_PACK_H__
