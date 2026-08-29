@echo off
REM LutTable regression harness - build and run.
REM
REM LutTable.cpp is compiled directly (it has no D3D dependency), so this
REM does not need the solution built first.
REM
REM (ASCII only on purpose: cmd.exe parses batch files in the OEM codepage.)

setlocal

set OUT=%~dp0out

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /utf-8 /MDd /W4 /std:c++17 /DUNICODE /D_UNICODE /DLUT_TEST_NO_PCH ^
   /Fe:"%OUT%\LutTest.exe" /Fo:"%OUT%\\" ^
   "%~dp0LutTest.cpp" "%~dp0..\LutTable.cpp"

if errorlevel 1 exit /b 1

"%OUT%\LutTest.exe"
exit /b %errorlevel%
