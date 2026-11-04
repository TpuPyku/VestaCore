@echo off
rem Builds and runs test_vhttp.exe (vhttp.cpp on the PC, VS2005 x86) in obj\ - against real servers, needs the
rem internet; exit code: the failed checks. BearSSL: %BR% (its inc\ and compat\ beside it) and %BRLIB%
rem (bearssl_pc.lib), by default OSMapsCE's (the sibling repo, build_bearssl.bat there).
rem "build": only builds. COVER=1: /Od /Zi (OpenCppCoverage, then crapper).
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if "%BR%"=="" set BR=%~dp0..\OSMapsCE\third_party\bearssl-0.6
if "%BRLIB%"=="" set BRLIB=%~dp0..\OSMapsCE\native_osm\obj\bearssl_pc.lib
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_vhttp.cpp vhttp.cpp /Foobj\ /Fdobj\ /Feobj\test_vhttp.exe ^
  /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%BR%\inc" /I"%BR%\..\compat" /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" "%BRLIB%" ws2_32.lib kernel32.lib advapi32.lib >obj\build.log || (type obj\build.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_vhttp.exe
