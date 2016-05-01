/*V6*/

#include <v6/core/common.h>
#include <v6/core/octree.h>

#include <v6/core/memory.h>
#include <v6/core/vec3.h>

BEGIN_V6_NAMESPACE

struct Node_s
{
	union
	{
		float	opacity;
		u32		firstChildID;
	};
};

struct StackItem_s
{	
	u32 idOffset;
	u32 count;
	u32 nodeID;
};

struct Octree_s
{
	GrowingAllocator_s allocator;
	IAllocator* heap;
	Node_s root;
	float radius;
};

static const u32 IS_CHILD_BIT	= 1u<<31;
static const u32 BATCH_SIZE		= 1024;
static const u32 STACK_MAX_SIZE	= 128;

// Private

static void Octree_AddPoints( Octree_s* octree, const Vec3* points, const float*, u32* ids, u32 count  )
{
	Vec3 split = Vec3_Make( 0.0f, 0.0f, 0.0f );
	Node_s node = octree->root;
	StackItem_s stack[STACK_MAX_SIZE] = {};
	u32 stackSize = 0;
	if ( node.firstChildID & IS_CHILD_BIT )
	{
		u32 octantIds[8][BATCH_SIZE];
		u32 octantCounts[8] = {};
		for ( u32 pointRank = 0; pointRank < count; ++count)
		{
			const u32 id = ids[pointRank];
			const Vec3& point = points[id];
			const u32 xSide = point.x < split.x ? 0u : 1u;
			const u32 ySide = point.y < split.y ? 0u : 2u;
			const u32 zSide = point.z < split.z ? 0u : 4u;
			const u32 octant = xSide | ySide | zSide;
			octantIds[octant][++octantCounts[octant]] = id;
		}
		const u32 childOffset = node.firstChildID & ~IS_CHILD_BIT;
		u32 idOffset = 0;		
		for ( u32 octant = 0; octant < 8; ++octant )
		{
			if ( !octantCounts[octant] )
				continue;
			memcpy( ids + idOffset, octantIds[octant], octantCounts[octant] * sizeof( u32 ) );
			V6_ASSERT( stackSize < STACK_MAX_SIZE );
			stack[stackSize++] = { idOffset, octantCounts[octant], childOffset + octant };
			idOffset += octantCounts[octant];
		}		
	}
}

// API

void Octree_Create( Octree_s** out_octree, float width, IAllocator* heap )
{
	Octree_s* octree = (Octree_s*)heap->alloc( sizeof( Octree_s ) );
	octree->heap = heap;
	GrowingAllocator_Create( &octree->allocator, heap );
	octree->root.opacity = 0.0f;
	octree->radius = width * 0.5f;

	*out_octree = octree;	
}

void Octree_Release( Octree_s* octree )
{
	IAllocator* heap = octree->allocator.heap;
	GrowingAllocator_Release( &octree->allocator );
	heap->free( octree );
	
	memset( octree, 0, sizeof( Octree_s ) );
}

void Octree_AddPoints( Octree_s* octree, const Vec3* points, const float* radii, u32 count, const u32 firstID )
{	
	for ( u32 pointRank = 0; pointRank < count; pointRank += BATCH_SIZE )
	{
		u32 stackIDs[BATCH_SIZE];		
		const u32 stackPointCount = Min( BATCH_SIZE, count - pointRank );		

		for ( u32 stackPointRank = 0; stackPointRank < stackPointCount; ++stackPointRank )
			stackIDs[stackPointRank] = firstID + pointRank + stackPointRank;

		Octree_AddPoints( octree, points + pointRank, radii + pointRank, stackIDs, stackPointCount );
	}
}

END_V6_NAMESPACE
