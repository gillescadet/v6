/*V6*/

#pragma once

#ifndef __V6_CORE_OCTREE_H__
#define __V6_CORE_OCTREE_H__

BEGIN_V6_CORE_NAMESPACE

class IHeap;
struct Octree_s;
struct Vec3;

void Octree_Create( Octree_s** octree, float width, IHeap* heap );
void Octree_Release( Octree_s* octree );
void Octree_AddPoints( Octree_s* octree, const Vec3* points, const float* radii, u32 count, const u32 firstID );

END_V6_CORE_NAMESPACE

#endif // __V6_CORE_OCTREE_H__