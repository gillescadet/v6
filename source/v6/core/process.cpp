/*V6*/

#include <v6/core/common.h>

#include <v6/core/windows_begin.h>
#include <windows.h>
#include <v6/core/windows_end.h>

#include <v6/core/process.h>

BEGIN_V6_NAMESPACE

bool Process_Execute( int* returnCode, const char* cmd )
{
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
		
	WaitForSingleObject( processInformation.hProcess, INFINITE);
	if ( returnCode )
		GetExitCodeProcess( processInformation.hProcess, (::DWORD*)returnCode );

	CloseHandle( processInformation.hProcess );
	CloseHandle( processInformation.hThread );
		
	return true;
}

END_V6_NAMESPACE
