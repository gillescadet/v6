/*V6*/

#include <v6/core/common.h>

#include <v6/core/filemap.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>
#include <v6/core/thread.h>

#define FILEMAP_CLUSTER_SHIFT		23
#define FILEMAP_CLUSTER_SIZE		(1 << FILEMAP_CLUSTER_SHIFT)
#define FILEMAP_CLUSTER_MASK		(FILEMAP_CLUSTER_SIZE-1)

#define FILEMAP_EMPTY_CACHE_EMPTY	0xFFFFFFFF

BEGIN_V6_NAMESPACE

enum
{
	FILEMAP_REGION_STATE_FREE,
	FILEMAP_REGION_STATE_ALLOCATED,
	FILEMAP_REGION_STATE_LOCKING,
	FILEMAP_REGION_STATE_LOCKED,
};

static const CPUEventID_t s_cpuEventFilemap		= CPUEvent_Register( "Filemap", false );

static bool FilemapQueue_Push( FilemapQueue_s* queue, u32 regionID )
{
	V6_ASSERT( Atomic_Load( &queue->begin ) <= queue->end );

	if ( queue->end - Atomic_Load( &queue->begin ) == FilemapQueue_s::REGION_MAX_COUNT )
		return false;

	const u32 end = queue->end & FilemapQueue_s::REGION_MOD_COUNT;
	queue->regionIDs[end] = regionID;

	V6_WRITE_BARRIER();
	++queue->end;

	return true;
}

static u32 FilemapQueue_BeginPop( FilemapQueue_s* queue )
{
	V6_ASSERT( queue->begin <= Atomic_Load( &queue->end ) );

	if ( queue->begin == Atomic_Load( &queue->end ) )
		return Filemap_s::INVALID_REGION;

	const u32 begin = queue->begin & FilemapQueue_s::REGION_MOD_COUNT;
	const u32 regionID = queue->regionIDs[begin];

	return regionID;
}

static void FilemapQueue_EndPop( FilemapQueue_s* queue )
{
	V6_ASSERT( queue->begin < Atomic_Load( &queue->end ) );

	V6_WRITE_BARRIER();
	++queue->begin;
}

static void FilemapQueue_Clear( FilemapQueue_s* queue )
{
	const u64 end = Atomic_Load( &queue->end );
	V6_ASSERT( queue->begin <= end );

	V6_WRITE_BARRIER();
	queue->begin = end;
}

static unsigned long __stdcall Filemap_ThreadLoop( void* fileMapPointer )
{
	Filemap_s* filemap = (Filemap_s*)fileMapPointer;

	while ( !Atomic_Load( &filemap->stop ) )
	{
		V6_CPU_EVENT_SCOPE( s_cpuEventFilemap );

		// process all regions to unlock

		for (;;)
		{
			const u32 regionID = FilemapQueue_BeginPop( &filemap->regionToUnlockQueue );
			if ( regionID == Filemap_s::INVALID_REGION )
				// nothing to unlock
				break;

			if ( regionID == Filemap_s::CANCEL_ALL_REGIONS )
			{
				FilemapQueue_Clear( &filemap->regionToUnlockQueue );
				FilemapQueue_Clear( &filemap->regionToLockQueue );
				filemap->pendingRegionToLock = Filemap_s::INVALID_REGION;
				break;
			}

			FilemapRegion_s* region = &filemap->regions[regionID];

			V6_ASSERT( region->state != FILEMAP_REGION_STATE_FREE );

			if ( region->state == FILEMAP_REGION_STATE_LOCKING || region->state == FILEMAP_REGION_STATE_LOCKED )
			{
				const u32 regionMask = 1 << regionID;

				const u32 cacheOffsetBegin = (u32)(region->offset >> FILEMAP_CLUSTER_SHIFT);
				const u32 cacheOffsetEnd = (u32)((region->offset + region->size + FILEMAP_CLUSTER_SIZE - 1) >> FILEMAP_CLUSTER_SHIFT);
				
				const u32 cacheEntryBegin = cacheOffsetBegin % filemap->streamCache.entryCount;
				const u32 cacheEntryCount = cacheOffsetEnd - cacheOffsetBegin;

				for ( u32 cacheEntryRank = 0; cacheEntryRank < cacheEntryCount; ++cacheEntryRank )
				{
					const u32 cacheEntry = (cacheEntryBegin + cacheEntryRank) % filemap->streamCache.entryCount;
					
					if ( filemap->streamCache.regionMasks[cacheEntry] & regionMask )
					{
						V6_ASSERT( filemap->streamCache.offsets[cacheEntry] == cacheOffsetBegin + cacheEntryRank );
						filemap->streamCache.regionMasks[cacheEntry] &= ~regionMask;
					}
				}

				if ( filemap->pendingRegionToLock == regionID )
					filemap->pendingRegionToLock = Filemap_s::INVALID_REGION;
			}
			else
			{
				V6_ASSERT( region->state == FILEMAP_REGION_STATE_ALLOCATED );
			}

			FilemapQueue_EndPop( &filemap->regionToUnlockQueue );

			if ( region->state == FILEMAP_REGION_STATE_LOCKING || region->state == FILEMAP_REGION_STATE_LOCKED )
			{
				V6_WRITE_BARRIER();
				region->state = FILEMAP_REGION_STATE_FREE;
			}
		}

		// process one cache entry for the pending region to lock

		if ( filemap->pendingRegionToLock == Filemap_s::INVALID_REGION )
			filemap->pendingRegionToLock = FilemapQueue_BeginPop( &filemap->regionToLockQueue );

		const u32 regionID = filemap->pendingRegionToLock;
			
		if ( regionID == Filemap_s::INVALID_REGION )
		{
			// nothing to lock
			Thread_Sleep( 1 );
			continue;
		}

		FilemapRegion_s* region = &filemap->regions[regionID];

		V6_ASSERT( region->state != FILEMAP_REGION_STATE_FREE && region->state != FILEMAP_REGION_STATE_LOCKED );
		if ( region->state == FILEMAP_REGION_STATE_ALLOCATED )
			region->state = FILEMAP_REGION_STATE_LOCKING;

		const u32 regionMask = 1 << regionID;

		const u32 cacheOffsetBegin = (u32)(region->offset >> FILEMAP_CLUSTER_SHIFT);
		const u32 cacheOffsetEnd = (u32)((region->offset + region->size + FILEMAP_CLUSTER_SIZE - 1) >> FILEMAP_CLUSTER_SHIFT);
			
		const u32 cacheOffset = cacheOffsetBegin + region->cacheEntryOffset;
		const u32 cacheEntry = cacheOffset % filemap->streamCache.entryCount;

		if ( filemap->streamCache.offsets[cacheEntry] != cacheOffset )
		{
			if ( filemap->streamCache.regionMasks[cacheEntry] )
			{
				// the cache is full
				static bool s_showMessage = true;
				if ( s_showMessage )
				{
					V6_WARNING( "Filemap cache is full.\n" );
					s_showMessage = false;
				}
				V6_ASSERT( (filemap->streamCache.regionMasks[cacheEntry] & regionMask) == 0 ); // cache is too small
				Thread_Sleep( 1 );
				continue;
			}

			filemap->streamReader->SetPos( ToX64( cacheOffset << FILEMAP_CLUSTER_SHIFT ) );
			filemap->streamReader->Read( ToX64( FILEMAP_CLUSTER_SIZE ), filemap->streamCache.buffer + (cacheEntry << FILEMAP_CLUSTER_SHIFT) );

			filemap->streamCache.offsets[cacheEntry] = cacheOffset;
		}

		filemap->streamCache.regionMasks[cacheEntry] |= regionMask;

		++region->cacheEntryOffset;

		const u32 cacheEntryCount = cacheOffsetEnd - cacheOffsetBegin;
		if ( region->cacheEntryOffset == cacheEntryCount )
		{
			const u32 cacheEntryBegin = cacheOffsetBegin % filemap->streamCache.entryCount;
			const int wrappedEntryCount = cacheEntryBegin + cacheEntryCount - filemap->streamCache.entryCount;
			if ( wrappedEntryCount > 0 )
				memcpy( filemap->streamCache.buffer + (filemap->streamCache.entryCount << FILEMAP_CLUSTER_SHIFT), filemap->streamCache.buffer, wrappedEntryCount << FILEMAP_CLUSTER_SHIFT );

			filemap->pendingRegionToLock = Filemap_s::INVALID_REGION;
			FilemapQueue_EndPop( &filemap->regionToLockQueue );

			V6_WRITE_BARRIER();
			region->state = FILEMAP_REGION_STATE_LOCKED;
		}
	}

	Signal_Emit( &filemap->endSignal );

	return 0;
}

void Filemap_Create( Filemap_s* filemap, u8* cache, u32 cacheCapacity )
{
	V6_ASSERT( cacheCapacity >= 2 * FILEMAP_CLUSTER_SIZE );

	memset( filemap, 0, sizeof( *filemap ) );

	memset( filemap->streamCache.offsets, 0xFF, sizeof( filemap->streamCache.offsets ) );

	u8* alignedCache = (u8*)(((uintptr_t)cache + FILEMAP_CLUSTER_SIZE - 1) & ~((uintptr_t)FILEMAP_CLUSTER_SIZE - 1));
	cacheCapacity -= (u32)(alignedCache - cache);
	filemap->streamCache.buffer = alignedCache;
	filemap->streamCache.entryCount = (cacheCapacity >> FILEMAP_CLUSTER_SHIFT) >> 1; // half of the cache is used for wrapped region
	V6_ASSERT( filemap->streamCache.entryCount > 0 && filemap->streamCache.entryCount <= FilemapCache_s::CACHE_ENTRY_MAX_COUNT );

	filemap->streamReader = nullptr;

	Signal_Create( &filemap->endSignal );

	filemap->pendingRegionToLock = Filemap_s::INVALID_REGION;

	Thread_Create( Filemap_ThreadLoop, filemap, 1 );
}

void Filemap_ClearCache( Filemap_s* filemap )
{
	Filemap_WaitForIdle( filemap );
	memset( filemap->streamCache.offsets, 0xFF, sizeof( filemap->streamCache.offsets ) );
	memset( filemap->streamCache.regionMasks, 0, sizeof( filemap->streamCache.regionMasks ) );
}

u32 Filemap_GetPendingRegionCount( Filemap_s* filemap )
{
	return (u32)(filemap->regionToLockQueue.end - Atomic_Load( &filemap->regionToLockQueue.begin ));
}
void Filemap_CancelAllRegions( Filemap_s* filemap )
{
	const bool pushed = FilemapQueue_Push( &filemap->regionToUnlockQueue, Filemap_s::CANCEL_ALL_REGIONS );
	V6_ASSERT( pushed );

	Filemap_ClearCache( filemap );

	memset( filemap->regions, 0, sizeof( filemap->regions ) );
}

void Filemap_SetStreamReader( Filemap_s* filemap, IStreamReader* streamReader )
{
	V6_ASSERT( filemap->regionToLockQueue.begin == filemap->regionToLockQueue.end );
	Filemap_ClearCache( filemap );
	filemap->streamReader = streamReader;
}

void* Filemap_GetRegionData( Filemap_s* filemap, u32 regionID )
{
	V6_ASSERT( regionID < FilemapQueue_s::REGION_MAX_COUNT );
	FilemapRegion_s* region = &filemap->regions[regionID];
	V6_ASSERT( region->state != FILEMAP_REGION_STATE_FREE );
	
	if ( region->state == FILEMAP_REGION_STATE_LOCKED )
	{
		const u32 cacheOffsetBegin = (u32)(region->offset >> FILEMAP_CLUSTER_SHIFT);
		const u32 cacheEntryBegin = cacheOffsetBegin % filemap->streamCache.entryCount;
		return filemap->streamCache.buffer + (cacheEntryBegin << FILEMAP_CLUSTER_SHIFT) + (region->offset & FILEMAP_CLUSTER_MASK);
	}

	return nullptr;
}

u32 Filemap_LockRegion( Filemap_s* filemap, u64 offset, u32 size )
{
	const u32 cacheOffsetBegin = (u32)(offset >> FILEMAP_CLUSTER_SHIFT);
	const u32 cacheOffsetEnd = (u32)((offset + size + FILEMAP_CLUSTER_SIZE - 1) >> FILEMAP_CLUSTER_SHIFT);
	const u32 cacheEntryCount = cacheOffsetEnd - cacheOffsetBegin;
	V6_ASSERT( cacheEntryCount > 0 );
	V6_ASSERT( cacheEntryCount <= filemap->streamCache.entryCount );

	for ( u32 regionID = 0; regionID < FilemapQueue_s::REGION_MAX_COUNT; ++regionID )
	{
		if ( filemap->regions[regionID].state == FILEMAP_REGION_STATE_FREE )
		{
			FilemapRegion_s* region = &filemap->regions[regionID];
			region->offset = offset;
			region->size = size;
			region->cacheEntryOffset = 0;
			region->state = FILEMAP_REGION_STATE_ALLOCATED;
			++region->version;

			const bool pushed = FilemapQueue_Push( &filemap->regionToLockQueue, regionID );
			V6_ASSERT( pushed );

			return regionID;
		}
	}

	V6_ASSERT_NOT_SUPPORTED();
	return Filemap_s::INVALID_REGION;
}

void Filemap_Release( Filemap_s* filemap )
{
	V6_ASSERT( filemap->regionToLockQueue.begin == filemap->regionToLockQueue.end );
	V6_ASSERT( filemap->regionToUnlockQueue.begin == filemap->regionToUnlockQueue.end );

	filemap->stop = true;

	Signal_Wait( &filemap->endSignal );
	Signal_Release( &filemap->endSignal );
}

void Filemap_UnlockRegion( Filemap_s* filemap, u32 regionID )
{
	V6_ASSERT( regionID < FilemapQueue_s::REGION_MAX_COUNT );
	FilemapRegion_s* region = &filemap->regions[regionID];
	V6_ASSERT( region->state != FILEMAP_REGION_STATE_FREE );

	const bool pushed = FilemapQueue_Push( &filemap->regionToUnlockQueue, regionID );
	V6_ASSERT( pushed );
}

void Filemap_WaitForIdle( Filemap_s* filemap )
{
	while ( Atomic_Load( &filemap->regionToLockQueue.begin ) < filemap->regionToLockQueue.end || Atomic_Load( &filemap->regionToUnlockQueue.begin ) < filemap->regionToUnlockQueue.end )
		Thread_Switch();
	V6_ASSERT( filemap->regionToLockQueue.begin == filemap->regionToLockQueue.end );
	V6_ASSERT( filemap->regionToUnlockQueue.begin == filemap->regionToUnlockQueue.end );
}

END_V6_NAMESPACE