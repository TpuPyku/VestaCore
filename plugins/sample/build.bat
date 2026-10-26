@echo off
rem Builds the sample add-on: Sample.exe (Windows CE, ARMv4) and SamplePC.exe (desktop, next to VestaHomePC.exe) with
rem Visual Studio 2005. To try it: copy into VestaHome\Plugins\Sample\ as Sample.exe (VestaHome starts it).
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=out

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist obj\pc  mkdir obj\pc
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c sample.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\Sample.exe" ^
  obj\arm\sample.obj /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === x86 (desktop)
"%VC%\bin\cl.exe" /nologo /O2 /W3 /MT /TP /c sample.cpp /Foobj\pc\ ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include"
if errorlevel 1 goto fail
"%VC%\bin\link.exe" /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\SamplePC.exe" obj\pc\sample.obj ^
  /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" user32.lib gdi32.lib kernel32.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1
