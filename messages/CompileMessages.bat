@echo off

:: load the environment of Developer Command Prompt
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"

mc -U EventLogMessages.mc
rc -r EventLogMessages.rc
link -dll -noentry /MACHINE:X64 -out:EventLogMessages.dll EventLogMessages.res
copy EventLogMessages.h ..\src
copy EventLogMessages.dll ..\x64\Debug
copy EventLogMessages.dll ..\x64\Release
pause