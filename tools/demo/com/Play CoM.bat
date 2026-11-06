@echo off
rem openciv4 test build: Chronicles of Mankind in the 64-bit engine.
rem   Play CoM.bat                                        the menu, the mod in Beyond the Sword\Mods
rem   Play CoM.bat "D:\Mods\Chronicles of Mankind"        the mod in another folder (or drop it onto this file)
rem   Play CoM.bat "" "D:\Saves\MyGame.CivBeyondSwordSave" continue a saved game
rem Beyond the Sword is found by itself (its installer's registry key, or Steam); else add
rem --civ4 "<the folder with Assets and Beyond the Sword>" to the lines below.
rem The log of a run goes to openciv4.log beside this file.
setlocal
set "MOD=Mods\Chronicles of Mankind"
if not "%~1"=="" set "MOD=%~1"
if "%~2"=="" (
	"%~dp0oc4core.exe" --mod "%MOD%" --menu > "%~dp0openciv4.log" 2>&1
) else (
	"%~dp0oc4core.exe" --mod "%MOD%" --play --load "%~f2" > "%~dp0openciv4.log" 2>&1
)
if errorlevel 1 (
	echo.
	echo It stopped with an error: see openciv4.log beside this file ^(please send it^).
	echo If the mod is not in Beyond the Sword\Mods\Chronicles of Mankind, drop its folder onto
	echo "Play CoM.bat", or run: "Play CoM.bat" "D:\path\to\Chronicles of Mankind"
	pause
)
