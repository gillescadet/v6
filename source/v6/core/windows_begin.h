/*V6*/

#ifndef __V6_WINDOWS_BEGIN_H__
	#define __V6_WINDOWS_BEGIN_H__
#else
	#error windows_begin.h already called
#endif

#pragma warning( push, 3 )

#if V6_UE4_PLUGIN == 1
#pragma warning( disable : 4459 )
#endif // #if V6_UE4_PLUGIN == 1

#if V6_UE4_PLUGIN == 1
#define INT		::INT
#define UINT	::UINT
#define DWORD	::DWORD
#define FLOAT	::FLOAT
#endif // #if V6_UE4_PLUGIN == 1

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
