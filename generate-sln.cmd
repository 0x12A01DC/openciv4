@echo off
rem Generates a Visual Studio solution at build\vs2026\openciv4.sln (or vs2022).
rem The solution is generated, not committed: CMakeLists.txt is the source of truth.
rem Usage: generate-sln.cmd [vs2026|vs2022] [--open]
setlocal
set PRESET=%1
if "%PRESET%"=="" set PRESET=vs2026
if "%PRESET%"=="--open" (
	set PRESET=vs2026
	set OPEN=1
)
if "%2"=="--open" set OPEN=1
call "%~dp0tools\vsenv.cmd" || exit /b 1
cmake --preset %PRESET% -S "%~dp0." || exit /b 1
for %%F in ("%~dp0build\%PRESET%\openciv4.sln*") do set SLN=%%~fF
echo.
echo Solution: %SLN%
if defined OPEN start "" "%SLN%"
