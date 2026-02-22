/*V6*/

#pragma once

#ifndef __V6_CORE_FILEMAP_H__
#define __V6_CORE_FILEMAP_H__

#include <v6/core/math.h>
#include <v6/core/thread.h>

BEGIN_V6_NAMESPACE

class IStreamReader;

struct FilemapRegion_s
{
	u64				offset;
	u32				size;
	u16				cacheEntryOffset;
	u8				state;
	u8				version;
};

struct FilemapQueue_s
{
	static const u32	REGION_MAX_COUNT = 32;
	static const u32	REGION_MOD_COUNT = REGION_MAX_COUNT-1;
	u32					regionIDs[REGION_MAX_COUNT];
	u64					begin;
	u64					end;
};
V6_STATIC_ASSERT( IsPowOfTwo_ConstExpr( FilemapQueue_s::REGION_MAX_COUNT ) );

struct FilemapCache_s
{
	static const u32	CACHE_ENTRY_MAX_COUNT = 1024;
	u32					offsets[CACHE_ENTRY_MAX_COUNT];
	u32					regionMasks[CACHE_ENTRY_MAX_COUNT];
	u8*					buffer;
	u32					entryCount;
};

V6_STATIC_ASSERT( FilemapQueue_s::REGION_MAX_COUNT <= 32 );

struct Filemap_s
{
	static const u32	INVALID_REGION	= FilemapQueue_s::REGION_MAX_COUNT;
	static const u32	CANCEL_ALL_REGIONS = 0xFFFFFFFF;
	FilemapRegion_s		regions[FilemapQueue_s::REGION_MAX_COUNT];
	FilemapQueue_s		regionToLockQueue;
	FilemapQueue_s		regionToUnlockQueue;
	FilemapCache_s		streamCache;
	Signal_s			endSignal;
	IStreamReader*		streamReader;
	u32					pendingRegionToLock;
	b32					stop;
};

void	Filemap_CancelAllRegions( Filemap_s* filemap );
void	Filemap_ClearCache( Filemap_s* filemap );
void	Filemap_Create( Filemap_s* filemap, u8* cache, u32 cacheCapacity );
u32		Filemap_GetPendingRegionCount( Filemap_s* filemap );
void*	Filemap_GetRegionData( Filemap_s* filemap, u32 regionID );
u32		Filemap_LockRegion( Filemap_s* filemap, u64 offset, u32 size );
void	Filemap_Release( Filemap_s* filemap );
void	Filemap_SetStreamReader( Filemap_s* filemap, IStreamReader* streamReader );
void	Filemap_UnlockRegion( Filemap_s* filemap, u32 regionID );
void	Filemap_WaitForIdle( Filemap_s* filemap );

END_V6_NAMESPACE

#endif // __V6_CORE_FILEMAP_H__