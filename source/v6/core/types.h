/*V6*/

#pragma once

#ifndef __V6_CORE_TYPES_H__
#define __V6_CORE_TYPES_H__

BEGIN_V6_NAMESPACE

typedef signed char			s8;
typedef unsigned char		u8;
typedef unsigned char		b8;

typedef signed short		s16;
typedef unsigned short		u16;
typedef unsigned short		b16;
struct hex16 { u16 n; };

typedef signed int			s32;
typedef unsigned int		u32;
typedef unsigned int		b32;
struct hex32 { u32 n; };

typedef signed long long	s64;
typedef unsigned long long	u64;
typedef unsigned long long	b64;
struct hex64 { u64 n; };

END_V6_NAMESPACE

#endif // __V6_CORE_TYPES_H__