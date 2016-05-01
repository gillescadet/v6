/*V6*/

#pragma once

#ifndef __V6_CORE_COMPUTE_H__
#define __V6_CORE_COMPUTE_H__

#include <v6/core/cpp_hlsl.h>

// HLSL API

BEGIN_V6_HLSL_NAMESPACE

#define	groupshared

void		AllMemoryBarrierWithGroupSync();
uint		countbits( uint n );
uint		firstbithigh( uint value );
void		InterlockedAdd( uint& value, uint inc );
void		InterlockedAdd( uint& value, uint inc, uint& prevValue );
void		InterlockedAnd( uint& value, uint mask );
void		InterlockedAnd( uint& value, uint mask, uint& prevValue );
void		InterlockedOr( uint& value, uint mask );
void		InterlockedOr( uint& value, uint mask, uint& prevValue );

END_V6_HLSL_NAMESPACE

BEGIN_V6_NAMESPACE

// Compute API

typedef void (*Compute_DispatchKernel_f)( u32 groupID, u32 threadGroupID, u32 threadID );

void		Compute_Dispatch( u32 elementCount, u32 groupSize, Compute_DispatchKernel_f kernel, const char* name );
void		Compute_Init();
void		Compute_Release();

END_V6_NAMESPACE

#endif // __V6_CORE_COMPUTE_H__
