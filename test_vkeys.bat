@echo off
rem Builds and runs test_vkeys.exe (vkeys.cpp on the PC, VS2005 x86) in obj\; exit code: the failed checks.
rem "build": only builds (the mutator runs it). COVER=1: /Od /Zi (OpenCppCoverage, then crapper).
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_vkeys.cpp vkeys.cpp /Foobj\ /Fdobj\ /Feobj\test_vkeys.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" user32.lib gdi32.lib kernel32.lib >obj\build.log || (type obj\build.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_vkeys.exe
