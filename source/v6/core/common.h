/*V6*/

#pragma once

#pragma warning( disable: 4514 4710 4711 )

#ifndef __V6_CORE_COMMON_H__
#define __V6_CORE_COMMON_H__

#define BEGIN_V6_CORE_NAMESPACE		namespace v6 { namespace core {
#define END_V6_CORE_NAMESPACE		} }

#if 0
#define __ASSERT( EXP )				assert( EXP )
#else
#define __ASSERT( EXP )				{ if ( !(EXP) ) __debugbreak(); }
#endif

#define V6_ASSERT( EXP )			__ASSERT( EXP )
#define V6_ASSERT_ALWAYS( EXP )		__ASSERT( false && (EXP) )
#define V6_ASSERT_NOT_SUPPORTED()	V6_ASSERT_ALWAYS( "Not supported" )

#define V6_STATIC_ASSERT( EXP )		static_assert( EXP, "static assert: "#EXP )

#define V6_PRINT( ... )				printf( __VA_ARGS__ );
#define V6_MSG( ... )				do { V6_PRINT( "[MSG] " ); V6_PRINT( __VA_ARGS__ ); } while ( false )
#define V6_WARNING( ... )			do { V6_PRINT( "[WRN] " ); V6_PRINT( __VA_ARGS__ ); } while ( false )
#define V6_ERROR( ... )				do { V6_PRINT( "[ERR] " ); V6_PRINT( __VA_ARGS__ ); } while ( false )

#define V6_INLINE					__inline

#pragma warning( push, 3 )
#ifdef _CRTBLD
#ifndef _ASSERT_OK
#error assert.h not for CRT internal use, use dbgint.h
#endif  /* _ASSERT_OK */
#include <cruntime.h>
#endif  /* _CRTBLD */
#include <crtdefs.h>
#include <float.h>
#include <intrin.h>
#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma warning( pop )

#include <v6/core/types.h>

#undef assert

#ifdef NDEBUG

#define assert(_Expression)     ((void)0)

#else  /* NDEBUG */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

_CRTIMP void __cdecl _wassert(_In_z_ const wchar_t * _Message, _In_z_ const wchar_t *_File, _In_ unsigned _Line);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#define assert(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), (core::u32)__LINE__), 0) )

#endif  /* NDEBUG */

#endif // __V6_CORE_COMMON_H__