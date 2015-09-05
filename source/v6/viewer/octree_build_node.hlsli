/*V6*/

#ifndef __V6_HLSL_OCTREE_BUILD_NODE_H__
#define __V6_HLSL_OCTREE_BUILD_NODE_H__

#define HLSL

#include "common_shared.h"

StructuredBuffer< Sample > samples				: register( HLSL_SORTED_SAMPLE_SRV );
RWBuffer< uint > sampleNodeOffsets 				: register( HLSL_SAMPLE_NODE_OFFSET_UAV );
RWStructuredBuffer< uint > firstChildOffsets	: register( HLSL_FIRST_CHILD_OFFSET_UAV );
RWBuffer< uint > sampleIndirectArgs 			: register( HLSL_SORTED_SAMPLE_INDIRECT_ARGS_UAV );
RWStructuredBuffer< OctreeLeaf > octreeLeaves	: register( HLSL_OCTREE_LEAF_UAV );

#endif // __V6_HLSL_OCTREE_BUILD_NODE_H__
