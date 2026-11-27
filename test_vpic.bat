@echo off
rem Builds and runs test_vpic.exe (vpic.cpp on the PC, VS2005 x86) in obj\; exit code: the failed checks.
rem TJpgDec and stb_image: VestaHome's by default (the sibling repo; TJPG, STB to change); the JPEG checks use
rem VestaHome's test photos (make_testmedia.py) when they are there.
rem "build": only builds (the mutator runs it). COVER=1: /Od /Zi (OpenCppCoverage, then crapper).
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
if "%TJPG%"=="" set TJPG=%~dp0..\VestaHome\third_party\tjpgd
if "%STB%"=="" set STB=%~dp0..\VestaHome\VestaHome\native
if "%PHOTOS%"=="" set PHOTOS=%~dp0..\VestaHome\VestaHome\VestaHome\TestMedia\Photos
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /c /TC "%TJPG%\tjpgd.c" /I"%TJPG%" /Foobj\tjpgd_pc.obj /I"%VC%\include" >obj\build.log || (type obj\build.log & exit /b 1)
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT test_vpic.cpp vpic.cpp obj\tjpgd_pc.obj /Foobj\ /Fdobj\ /Feobj\test_vpic.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%TJPG%" /I"%STB%" /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" user32.lib gdi32.lib kernel32.lib >obj\build.log || (type obj\build.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_vpic.exe "%PHOTOS%"
