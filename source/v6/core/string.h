/*V6*/

#pragma once

#ifndef __V6_CORE_STRING_H__
#define __V6_CORE_STRING_H__

BEGIN_V6_NAMESPACE

const char* String_Format( const char* format, ... );
const char* String_FormatInteger( u32 n );
void		String_ResetInternalBuffer();

END_V6_NAMESPACE

#endif // __V6_CORE_STRING_H__
