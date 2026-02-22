/*V6*/

#pragma once

#pragma warning( disable: 4514 4710 4711 )

#ifndef __V6_CORE_COMMON_H__
#define __V6_CORE_COMMON_H__

#define BEGIN_V6_NAMESPACE		namespace v6 {
#define END_V6_NAMESPACE		}

#if defined( PLATFORM_WINDOWS )
#define V6_UE4_PLUGIN			1
#else
#define V6_UE4_PLUGIN			0
#endif

enum
{
	MSG_DEV,
	MSG_LOG,
	MSG_WARNING,
	MSG_ERROR,
	MSG_FATAL,
};

#if _DEBUG
#define V6_DEBUG	1
#define V6_RELEASE	0
#define V6_RETAIL	0
#else
#define V6_DEBUG	0
#define V6_RELEASE	1
#define V6_RETAIL	0
#endif

#define V6_PRINT( MSG_TYPE, ... )	v6::OutputMessage( MSG_TYPE, __VA_ARGS__ )
#define V6_DEVMSG( ... )			do { V6_PRINT( MSG_DEV, __VA_ARGS__ ); } while ( false )
#define V6_MSG( ... )				do { V6_PRINT( MSG_LOG, __VA_ARGS__ ); } while ( false )
#define V6_WARNING( ... )			do { V6_PRINT( MSG_WARNING, __VA_ARGS__ ); } while ( false )
#define V6_ERROR( ... )				do { V6_PRINT( MSG_ERROR, __VA_ARGS__ ); } while ( false )
#define V6_FATAL( ... )				do { V6_PRINT( MSG_FATAL, __VA_ARGS__ ); exit( 1 ); } while ( false )

#if V6_DEBUG == 1
#define __ASSERT( EXP, TXT )		do { if ( !(EXP) ) __debugbreak(); } while ( false )
#else
#define __ASSERT( EXP, TXT )		do { if ( !(EXP) ) { V6_PRINT( MSG_FATAL, TXT ); } } while ( false )
#endif

#define V6_ASSERT( EXP )			__ASSERT( EXP, "Assertion failed: " #EXP )
#define V6_ASSERT_TXT( EXP, TXT )	__ASSERT( EXP, TXT )
#define V6_ASSERT_ALWAYS( TXT )		__ASSERT( false, #TXT )
#define V6_ASSERT_NOT_SUPPORTED()	V6_ASSERT_ALWAYS( "Not supported" )
#define V6_ASSERT_UNREACHABLE()		V6_ASSERT_ALWAYS( "Unreachable" )

#define V6_STATIC_ASSERT( EXP )		static_assert( EXP, "static assert: "#EXP )

#define V6_ALIGN( SIZE )			__declspec( align( SIZE ) )
#define V6_INLINE					__inline
#define V6_THREAD_LOCAL_STORAGE		__declspec( thread )

#define V6_ARRAY_COUNT( ARRAY )		( sizeof( ARRAY ) / sizeof( ARRAY[0] ) )

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
#include <stdarg.h>
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

#define assert(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), (u32)__LINE__), 0) )

#endif  /* NDEBUG */

BEGIN_V6_NAMESPACE

void OutputMessage( u32 msgType, const char* format, ... );

END_V6_NAMESPACE

#endif // __V6_CORE_COMMON_H__