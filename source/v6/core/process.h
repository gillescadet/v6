/*V6*/

#pragma once

#ifndef __V6_CORE_PROCESS_H__
#define __V6_CORE_PROCESS_H__

BEGIN_V6_NAMESPACE

struct Process_s
{
	void* processHandle;
	void* threadHandle;
};

void	Process_Cancel( Process_s* process );
bool	Process_Execute( int* returnCode, const char* cmd );
bool	Process_Launch( Process_s* process, const char* cmd );
int		Process_Wait( Process_s* process );

END_V6_NAMESPACE

#endif // __V6_CORE_PROCESS_H__
