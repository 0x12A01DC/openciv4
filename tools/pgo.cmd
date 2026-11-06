@echo off
rem Profile-guided build of the game core (CvGameCoreDLL) in build\Release:
rem   1. a normal build plays two games into the late game and saves them,
rem   2. an instrumented DLL (OC4_PGO=GEN) trains on the early turns of a new game and on a
rem      few turns from each late save (instrumented code runs ~30x slower, hence the saves),
rem   3. the optimised DLL is linked from the counts (OC4_PGO=USE).
rem Maps and seeds differ from any benchmark. Later builds of build\Release keep using the
rem profile; functions changed since then fall back to normal optimisation. Run it again
rem after larger changes, or configure with -DOC4_PGO=OFF to stop using it.
rem
rem Usage: tools\pgo.cmd "<Civ4 folder with Assets, Warlords and Beyond the Sword>"
setlocal
set CIV4=%~1
if "%CIV4%"=="" (
	echo usage: tools\pgo.cmd "<Civ4 install folder>"
	exit /b 1
)
set ROOT=%~dp0..
set BUILD=%ROOT%\build\Release
set PGO=%BUILD%\pgo
set CORE=%BUILD%\bin\oc4core.exe
call "%~dp0vsenv.cmd" || exit /b 1

echo === normal build, late-game saves for training
cmake --preset release -S "%ROOT%" -DOC4_PGO=OFF >nul || exit /b 1
cmake --build "%BUILD%" || exit /b 1
if not exist "%PGO%" mkdir "%PGO%"
"%CORE%" --civ4 "%CIV4%" --map Fractal --size STANDARD --seed 3 --turns 230 --save-at 220 --save "%PGO%\late-standard.CivBeyondSwordSave" >"%PGO%\gen-standard.txt" 2>&1 || exit /b 1
"%CORE%" --civ4 "%CIV4%" --map Pangaea --size LARGE --seed 5 --turns 190 --save-at 180 --save "%PGO%\late-large.CivBeyondSwordSave" >"%PGO%\gen-large.txt" 2>&1 || exit /b 1

echo === instrumented build and training
cmake --preset release -S "%ROOT%" -DOC4_PGO=GEN >nul || exit /b 1
cmake --build "%BUILD%" || exit /b 1
del /q "%PGO%\*.pgc" 2>nul
rem The instrumented DLL needs the PGO runtime and writes its counts next to the .pgd.
set PATH=%VCToolsInstallDir%bin\Hostx64\x64;%PATH%
set VCPROFILE_PATH=%PGO%
"%CORE%" --civ4 "%CIV4%" --map Continents --size STANDARD --seed 4 --turns 60 >"%PGO%\train-early.txt" 2>&1 || exit /b 1
"%CORE%" --civ4 "%CIV4%" --load "%PGO%\late-standard.CivBeyondSwordSave" --turns 4 >"%PGO%\train-standard.txt" 2>&1 || exit /b 1
"%CORE%" --civ4 "%CIV4%" --load "%PGO%\late-large.CivBeyondSwordSave" --turns 4 >"%PGO%\train-large.txt" 2>&1 || exit /b 1
pgomgr /merge "%PGO%\CvGameCoreDLL.pgd" >nul || exit /b 1

echo === optimised build
cmake --preset release -S "%ROOT%" -DOC4_PGO=USE >nul || exit /b 1
cmake --build "%BUILD%" || exit /b 1
echo done: %BUILD%\bin\CvGameCoreDLL-x64.dll
