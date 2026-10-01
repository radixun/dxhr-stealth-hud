@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0GFX\build-native"
cl /nologo /O2 /MT /EHsc /std:c++17 /I..\source ..\source\events_test.cc ..\source\StealthEvents.cpp /Fe:events_test.exe
if errorlevel 1 exit /b 1
events_test.exe
exit /b %errorlevel%
