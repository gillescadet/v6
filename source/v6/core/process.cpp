/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/process.h>

BEGIN_V6_NAMESPACE

bool Process_Launch( Process_s* process, const char* cmd )
{
	memset( process, 0, sizeof( *process ) );

	SECURITY_ATTRIBUTES securityAttributes = {};
	securityAttributes.nLength = sizeof( SECURITY_ATTRIBUTES );
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = true;

	STARTUPINFOA startupInfo = {};
	startupInfo.cb = sizeof( startupInfo );
    startupInfo.dwX = CW_USEDEFAULT;
    startupInfo.dwY = CW_USEDEFAULT;
    startupInfo.dwXSize = CW_USEDEFAULT;
    startupInfo.dwYSize = CW_USEDEFAULT;
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWNORMAL;

	PROCESS_INFORMATION processInformation = {};

	if ( !CreateProcessA( nullptr, (char *)cmd, &securityAttributes, &securityAttributes, true, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, nullptr, nullptr, &startupInfo, &processInformation ) )
	{
		V6_ERROR( "Failed to execute %s\n", cmd );
		return false;
	}
		
	process->processHandle = processInformation.hProcess;
	process->threadHandle = processInformation.hThread;
		
	return true;
}

int Process_Wait( Process_s* process )
{
	V6_ASSERT( process->processHandle );
	V6_ASSERT( process->threadHandle );

	WaitForSingleObject( process->processHandle, INFINITE );
	
	int returnCode;
	GetExitCodeProcess( process->processHandle, (::DWORD*)&returnCode );

	CloseHandle( process->processHandle );
	CloseHandle( process->threadHandle );
	
	memset( process, 0, sizeof( *process ) );

	return returnCode;
}

bool Process_Execute( int* returnCode, const char* cmd )
{
	Process_s process;

	if ( !Process_Launch( &process, cmd ) ) 
		return false;

	*returnCode = Process_Wait( &process );
	return true;
}

void Process_Cancel( Process_s* process )
{
	V6_ASSERT( process->processHandle );

	TerminateProcess( process->processHandle, 1 );

	CloseHandle( process->processHandle );
	CloseHandle( process->threadHandle );

	memset( process, 0, sizeof( *process ) );
}

END_V6_NAMESPACE
