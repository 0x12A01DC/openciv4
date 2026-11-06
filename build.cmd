@echo off
rem Builds openciv4 with MSVC x64 + Ninja using CMakePresets.json.
rem Usage: build.cmd [release|debug] [extra cmake --build args, e.g. --target CvGameCoreDLL -- -k 0]
setlocal
set PRESET=%1
if "%PRESET%"=="" set PRESET=release
if /i "%PRESET%"=="Release" set PRESET=release
if /i "%PRESET%"=="Debug" set PRESET=debug
set ARGS=
:collect
shift
if "%~1"=="" goto run
set ARGS=%ARGS% %1
goto collect
:run
call "%~dp0tools\vsenv.cmd" || exit /b 1
if not exist "%~dp0build\%PRESET%\build.ninja" (
	cmake --preset %PRESET% -S "%~dp0." || exit /b 1
)
cmake --build "%~dp0build\%PRESET%" %ARGS%
