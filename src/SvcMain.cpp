//======================================================================================================================
//  Skeleton of a Windows service. You shouldn't have to change anything here.
//======================================================================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

//#pragma comment(lib, "advapi32.lib")

#include <cstdio>
#include <signal.h>

#include "SvcCommon.hpp"
#include "EventLogMessages.h"
#include "MyService.hpp"

#ifndef DEBUGGING_PROCESS
void WINAPI SvcCtrlHandler( DWORD ctrlCode )
{
	switch(ctrlCode)
	{
		case SERVICE_CONTROL_STOP:
			ReportSvcStatus( SERVICE_STOP_PENDING, NO_ERROR, 2000 );
			ReportSvcEvent( SVCEVENT_STATUS_REPORT, _T("Stopping service") );
			MyServiceStop();
			break;
		case SERVICE_CONTROL_INTERROGATE:
			break;
		default:
			break;
	}
}
#else
void signalHandler( int )
{
	ReportSvcStatus( SERVICE_STOP_PENDING, NO_ERROR, 2000 );
	ReportSvcEvent( SVCEVENT_STATUS_REPORT, _T("Stopping service") );
	MyServiceStop();
}
#endif

void WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
#ifndef DEBUGGING_PROCESS
	// Register the handler function for the service
	g_svcStatusHandle = RegisterServiceCtrlHandler( MY_SERVICE_NAME, SvcCtrlHandler );
	if (!g_svcStatusHandle)
	{
		ReportSvcEvent( SVCEVENT_INIT_SYSCALL_ERROR, _T("RegisterServiceCtrlHandler failed (%u)"), unsigned(GetLastError()) );
		return;
	}
#else
	signal( SIGINT, signalHandler );
#endif

	// Report initial status to the SCM.
	// Within this number of milliseconds MyServiceInit() should report next status.
	ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 1000 );
	ReportSvcEvent( SVCEVENT_STATUS_REPORT, _T("Initializing service") );

	// Perform service-specific initialization.
	if (!MyServiceInit())
	{
		ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
		ReportSvcEvent( SVCEVENT_STATUS_REPORT, _T("Service failed to initialize") );
		return;
	}

	// Report running status when initialization is complete.
	ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );
	ReportSvcEvent( SVCEVENT_STATUS_REPORT, _T("Service is initialized and running") );

	MyServiceRun();

	MyServiceCleanup();

	ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
	ReportSvcEvent( SVCEVENT_STATUS_REPORT, _T("Service has stopped") );
}


int __cdecl _tmain( int argc, TCHAR * argv [] )
{
#ifndef DEBUGGING_PROCESS
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ (TCHAR*)MY_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL }
	};

	// This call returns when the service has stopped.
	// The process should simply terminate when the call returns.
	if (!StartServiceCtrlDispatcher( DispatchTable ))
	{
		ReportSvcEvent( SVCEVENT_INIT_SYSCALL_ERROR, _T("StartServiceCtrlDispatcher failed (%u)"), unsigned(GetLastError()) );
		fprintf( stderr, "This program is a service and cannot run directly.\n" );
		return 1;
	}
#else
	SvcMain( argc, argv );
#endif

	return 0;
}
