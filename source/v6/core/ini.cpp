/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/ini.h>

BEGIN_V6_NAMESPACE

void Ini_Init( Ini_s* ini, const char* filename )
{
	memset( ini, 0, sizeof( *ini ) );
	strcpy_s( ini->filename, sizeof( ini->filename ), filename );
}

const char* Ini_ReadKey( Ini_s* ini, const char* section, const char* key, const char* defaulValue )
{
	GetPrivateProfileStringA( section, key, defaulValue, ini->valueBuffer, sizeof( ini->valueBuffer ), ini->filename );
	return ini->valueBuffer;
}

int Ini_ReadKey( Ini_s* ini, const char* section, const char* key, int defaulValue )
{
	return (int)GetPrivateProfileIntA( section, key, defaulValue, ini->filename );
}

void Ini_Release( Ini_s* ini)
{
	memset( ini, 0, sizeof( *ini ) );
}

void Ini_WriteKey( Ini_s* ini, const char* section, const char* key, const char* value )
{
	WritePrivateProfileStringA( section, key, value, ini->filename );
}

void Ini_WriteKey( Ini_s* ini, const char* section, const char* key, int value )
{
	char str[16];
	_itoa_s( value, str, sizeof( str ), 10 );
	WritePrivateProfileStringA( section, key, str, ini->filename );
}

//--------------------------------------------------------------------------------

END_V6_NAMESPACE
