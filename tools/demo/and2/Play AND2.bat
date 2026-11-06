@echo off
rem openciv4 demo: Rise of Mankind - A New Dawn 2 (SVN revision 1091) in the 64-bit engine.
rem   Play AND2.bat                                         the menu, the mod in Beyond the Sword\Mods
rem   Play AND2.bat "D:\Mods\Rise of Mankind - A New Dawn"  the mod in another folder
rem   Play AND2.bat "" "D:\Saves\MyGame.CivBeyondSwordSave" continue a saved game
rem Beyond the Sword is found by itself (its installer's registry key, or Steam); else add
rem --civ4 "<the folder with Assets and Beyond the Sword>" to the line below.
setlocal
set "MOD=Mods\Rise of Mankind - A New Dawn"
if not "%~1"=="" set "MOD=%~1"
if "%~2"=="" (
	"%~dp0oc4core.exe" --mod "%MOD%" --menu
) else (
	"%~dp0oc4core.exe" --mod "%MOD%" --play --load "%~f2"
)
if errorlevel 1 (
	echo.
	echo If the mod is not in Beyond the Sword\Mods, drop its folder onto "Play AND2.bat",
	echo or run: "Play AND2.bat" "D:\path\to\Rise of Mankind - A New Dawn"
	pause
)
