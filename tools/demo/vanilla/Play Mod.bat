@echo off
rem openciv4 demo: a mod in the 64-bit engine. Either a mod without a game core of its own
rem (XML, Python and art only), or one with CvGameCoreDLL-x64.dll in its Assets folder (see README.txt).
rem   Play Mod.bat "Mods\My Mod"                                       a mod in Beyond the Sword\Mods
rem   Play Mod.bat "D:\Mods\My Mod"                                    a mod elsewhere (or drop its folder onto this file)
rem   Play Mod.bat "D:\Mods\My Mod" "D:\Saves\MyGame.CivBeyondSwordSave" continue a saved game
rem The log of a run goes to openciv4.log beside this file.
setlocal
if "%~1"=="" (
	echo Drop the mod's folder onto "Play Mod.bat", or run: "Play Mod.bat" "D:\path\to\My Mod"
	pause
	exit /b 1
)
if "%~2"=="" (
	"%~dp0oc4core.exe" --mod "%~1" --menu > "%~dp0openciv4.log" 2>&1
) else (
	"%~dp0oc4core.exe" --mod "%~1" --play --load "%~f2" > "%~dp0openciv4.log" 2>&1
)
if errorlevel 1 (
	echo.
	echo It stopped with an error: see openciv4.log beside this file.
	pause
)
