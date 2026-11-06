@echo off
rem openciv4 demo: Civilization IV: Beyond the Sword in the 64-bit engine.
rem   Play Beyond the Sword.bat                                       the menu
rem   Play Beyond the Sword.bat "D:\Saves\MyGame.CivBeyondSwordSave"  continue a saved game (or drop it onto this file)
rem Beyond the Sword is found by itself (its installer's registry key, or Steam); else add
rem --civ4 "<the folder with Assets and Beyond the Sword>" to the lines below.
rem The log of a run goes to openciv4.log beside this file.
setlocal
if "%~1"=="" (
	"%~dp0oc4core.exe" --menu > "%~dp0openciv4.log" 2>&1
) else (
	"%~dp0oc4core.exe" --play --load "%~f1" > "%~dp0openciv4.log" 2>&1
)
if errorlevel 1 (
	echo.
	echo It stopped with an error: see openciv4.log beside this file.
	pause
)
