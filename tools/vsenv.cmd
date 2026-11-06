@echo off
rem Puts the newest Visual Studio (with C++ tools) x64 environment, CMake and Ninja on PATH.
if defined VSCMD_VER if "%VSCMD_ARG_TGT_ARCH%"=="x64" goto :tools
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
	echo vswhere.exe not found - install Visual Studio 2022/2026 with "Desktop development with C++".
	exit /b 1
)
set VSROOT=
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSROOT=%%I
if not defined VSROOT (
	echo No Visual Studio with C++ x64 tools found.
	exit /b 1
)
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
:tools
where cmake >nul 2>nul || set PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%
where ninja >nul 2>nul || set PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
exit /b 0
