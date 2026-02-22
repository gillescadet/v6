/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/string.h>

BEGIN_V6_NAMESPACE

thread_local char	s_strBuffer[32 * 1024];
thread_local char*	const s_strEnd = s_strBuffer + sizeof( s_strBuffer );
thread_local char*	s_str = s_strBuffer;

void String_ResetInternalBuffer()
{
	s_str = s_strBuffer;
}

const char* String_Format( const char* format, ... )
{
	char* const sBegin = s_str;

	va_list argptr;
	va_start( argptr, format );
	s_str += vsprintf_s( s_str, s_strEnd - s_str, format, argptr ) + 1;
	va_end( argptr );

	V6_ASSERT( s_str < s_strEnd );
	
	return sBegin;
}

const char* String_FormatInteger( u32 n )
{
	char* const sBegin = s_str;

	if ( n > 1000000000 )
	{
		const u32 billion = n / 1000000000;
		s_str += sprintf_s( s_str, s_strEnd - s_str, "%d,", billion );
		n -= billion * 1000000000;
	}
	if ( n > 1000000 )
	{
		const u32 million = n / 1000000;
		if ( s_str == sBegin )
			s_str += sprintf_s( s_str, s_strEnd - s_str, "%d,", million );
		else
			s_str += sprintf_s( s_str, s_strEnd - s_str, "%03d,", million );
		n -= million * 1000000;
	}
	if ( n > 1000 )
	{
		const u32 thousand = n / 1000;
		if ( s_str == sBegin )
			s_str += sprintf_s( s_str, s_strEnd - s_str, "%d,", thousand );
		else
			s_str += sprintf_s( s_str, s_strEnd - s_str, "%03d,", thousand );
		n -= thousand * 1000;
	}

	if ( s_str == sBegin )
		s_str += sprintf_s( s_str, s_strEnd - s_str, "%d", n );
	else
		s_str += sprintf_s( s_str, s_strEnd - s_str, "%03d", n );

	*s_str = 0;
	s_str += 1;

	V6_ASSERT( s_str < s_strEnd );

	return sBegin;
}

void String_ConvertWideCharToAnsiChar( char* ansiString, u32 ansiStringMaxSize, const wchar_t* wideString )
{
	WideCharToMultiByte( CP_ACP, 0, wideString, -1, ansiString, ansiStringMaxSize, nullptr, nullptr );
}

END_V6_NAMESPACE
