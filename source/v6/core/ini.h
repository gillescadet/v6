/*V6*/

#pragma once

#ifndef __V6_CORE_INI_H__
#define __V6_CORE_INI_H__

BEGIN_V6_NAMESPACE

struct Ini_s
{
	char	filename[256];
	char	valueBuffer[256];
};

void		Ini_Init( Ini_s* ini, const char* filename );
const char* Ini_ReadKey( Ini_s* ini, const char* section, const char* key, const char* defaulValue );
int			Ini_ReadKey( Ini_s* ini, const char* section, const char* key, int defaulValue );
void		Ini_Release( Ini_s* ini);
void		Ini_WriteKey( Ini_s* ini, const char* section, const char* key, const char* value );
void		Ini_WriteKey( Ini_s* ini, const char* section, const char* key, int value );

END_V6_NAMESPACE

#endif // __V6_CORE_INI_H__