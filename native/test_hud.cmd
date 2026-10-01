@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0GFX\build-native"
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS /I..\source ..\source\hud_smoke_test.cc ..\source\StealthHUD.cpp ..\source\StealthEvents.cpp ..\source\imgui\imgui.cpp ..\source\imgui\imgui_draw.cpp ..\source\imgui\imgui_widgets.cpp /Fe:hud_smoke_test.exe /link user32.lib advapi32.lib
if errorlevel 1 exit /b 1
hud_smoke_test.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /std:c++17 /I..\source ..\source\bonus_test.cc /Fe:bonus_test.exe
if errorlevel 1 exit /b 1
bonus_test.exe
exit /b %errorlevel%
