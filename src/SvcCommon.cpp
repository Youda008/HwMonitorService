//======================================================================================================================
//  Functions needed in SvcMain, but useful for your own code too.
//======================================================================================================================

#include "SvcCommon.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#include <ctime>
#include <cstdio>

#include "MyService.hpp" // only because of service name
#include "EventLogMessages.h"


SERVICE_STATUS_HANDLE g_svcStatusHandle;


void ReportSvcStatus( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint )
{
#ifndef DEBUGGING_PROCESS
	static DWORD dwCheckPoint = 1;

	SERVICE_STATUS g_svcStatus;

	g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_svcStatus.dwServiceSpecificExitCode = 0;

	g_svcStatus.dwCurrentState = dwCurrentState;
	g_svcStatus.dwWin32ExitCode = dwWin32ExitCode;
	g_svcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		g_svcStatus.dwControlsAccepted = 0;
	else
		g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
		g_svcStatus.dwCheckPoint = 0;
	else
		g_svcStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM
	SetServiceStatus( g_svcStatusHandle, &g_svcStatus );
#endif
}

static WORD GetTypeFromEventID( DWORD dwEventID )
{
	DWORD dwEventSeverity = (dwEventID & 0xC0000000) >> 30;
	switch (dwEventSeverity)
	{
		case STATUS_SEVERITY_SUCCESS:
			return EVENTLOG_SUCCESS;
		case STATUS_SEVERITY_INFORMATIONAL:
			return EVENTLOG_INFORMATION_TYPE;
		case STATUS_SEVERITY_WARNING:
			return EVENTLOG_WARNING_TYPE;
		case STATUS_SEVERITY_ERROR:
			return EVENTLOG_ERROR_TYPE;
		default:
			return EVENTLOG_WARNING_TYPE;
	}
}

void ReportSvcEvent( DWORD dwEventID, LPCTSTR szFormat, ... )
{
	va_list argList;
	va_start( argList, szFormat );

#ifndef DEBUGGING_PROCESS

	HANDLE hEventSource = RegisterEventSource( NULL, MY_SERVICE_NAME );

	if (hEventSource != NULL)
	{
		WORD wType = GetTypeFromEventID( dwEventID );
		WORD wCategory = 0;  // TODO

		TCHAR buffer [128];
		_vsntprintf( buffer, 127, szFormat, argList );

		LPCTSTR lpszStrings [2] = { MY_SERVICE_NAME, buffer };

		ReportEvent(
			hEventSource,        // event log handle
			wType,               // event type
			wCategory,           // event category
			dwEventID,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL                 // no binary data
		);

		DeregisterEventSource(hEventSource);
	}

#else

	TCHAR timeStr [20];
	time_t now = time(nullptr);
	_tcsftime( timeStr, 20, _T("%Y-%m-%d %H:%M:%S"), localtime(&now) );

	TCHAR finalFormat [128];
	_sntprintf( finalFormat, 127, _T("%s [Event ID: %04u] %s\n"), timeStr, dwEventID, szFormat );

	_vtprintf( finalFormat, argList );
	fflush( stdout );

#endif

	va_end( argList );
}
