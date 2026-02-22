/*V6*/

#pragma once

#ifndef __V6_CORE_PLATFORM_H__
#define __V6_CORE_PLATFORM_H__

#include <v6/core/mat4x4.h> 
#include <v6/core/vec3.h> 
#include <v6/core/vec3i.h> 

BEGIN_V6_NAMESPACE

bool	Platform_Init( const char* platformName, const char* appID );
bool	Platform_IsDevelopperMode();
bool	Platform_ProcessMessages();
void	Platform_Shutdown();

END_V6_NAMESPACE

#endif // __V6_CORE_PLATFORM_H__
