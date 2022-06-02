//======================================================================================================================
//  Functions needed in SvcMain, but useful for your own code too.
//======================================================================================================================

#ifndef SVC_COMMON_INCLUDED
#define SVC_COMMON_INCLUDED


#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // just because of DWORD ffs

extern SERVICE_STATUS_HANDLE g_svcStatusHandle;

void ReportSvcStatus( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint );

void ReportSvcEvent( DWORD dwEventID, LPCTSTR szFormat, ... );


#endif // SVC_COMMON_INCLUDED
