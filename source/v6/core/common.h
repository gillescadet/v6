/*V6*/

#pragma once

#ifndef __V6_CORE_COMMON_H__
#define __V6_CORE_COMMON_H__

#define BEGIN_ANONYMOUS_NAMESPACE	namespace {
#define END_ANONYMOUS_NAMESPACE		}

#define BEGIN_V6_CORE_NAMESPACE		namespace v6 { namespace core {
#define END_V6_CORE_NAMESPACE		} }

#define V6_ASSERT( EXP )			assert(EXP)
#define V6_PRINT( ... )				printf(__VA_ARGS__)
#define V6_LOG( ... )				{ printf("[LOG] "); printf(__VA_ARGS__); printf("\n"); }
#define V6_WARNING( ... )			{ printf("[WARNING] "); printf(__VA_ARGS__); printf("\n"); }
#define V6_ERROR( ... )				{ printf("[ERROR] "); printf(__VA_ARGS__); printf("\n"); }

#define V6_INLINE					__inline

#include <v6/core/types.h>

#include <stdio.h>
#include <assert.h>

#endif // __V6_CORE_COMMON_H__