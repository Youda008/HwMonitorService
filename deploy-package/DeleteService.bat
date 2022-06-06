@echo off
set SERVNAME=HwMonitorService
sc stop "%SERVNAME%"
sc delete "%SERVNAME%"
reg delete "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog\Application\%SERVNAME%" /f
pause
