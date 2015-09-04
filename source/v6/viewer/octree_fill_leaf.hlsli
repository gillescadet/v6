/*V6*/

#ifndef __V6_HLSL_OCTREE_FILL_LEAF_H__
#define __V6_HLSL_OCTREE_FILL_LEAF_H__

#define HLSL

#include "common_shared.h"

StructuredBuffer< Sample > samples				: register( HLSL_SORTED_SAMPLE_SRV );
Buffer< uint > sampleNodeOffsets 				: register( HLSL_SAMPLE_NODE_OFFSET_SRV );
RWStructuredBuffer< OctreeLeaf > octreeLeaves	: register( HLSL_OCTREE_LEAF_UAV );

#endif // __V6_HLSL_OCTREE_FILL_LEAF_H__
