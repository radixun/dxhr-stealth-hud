@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0GFX"
if not exist build-native mkdir build-native
cd build-native
cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /DWIN32 /DNDEBUG /DWIL_SUPPRESS_EXCEPTIONS /DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD /DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS /I..\source ..\source\*.cpp ..\source\effects\*.cpp ..\source\imgui\*.cpp ..\source\Utils\Patterns.cpp /link /DEF:..\source\DXHRDC-GFX.def /OUT:DXHRDC-GFX.asi d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib shlwapi.lib advapi32.lib
exit /b %errorlevel%
