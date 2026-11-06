openciv4 demo: Beyond the Sword, and your mods
==============================================

A 64-bit engine for Civilization IV: Beyond the Sword. It runs the game's own game core
(CvGameCoreDLL, the SDK Firaxis released, rebuilt as 64-bit) and the game's Python scripts
as they are, and does the rest itself: the map, the interface, the leaders, sound and music.

This demo plays the unmodded game, mods that have no game core of their own, and mods whose
game core you have ported to 64-bit yourself.

This is a work in progress demo, not a release.

Problems, crashes, questions: https://github.com/bssth/openciv4/issues
(attach openciv4.log from this folder and say what you did).


What you need
-------------
- Windows 10 or 11, 64-bit, with a Direct3D 12 graphics card.
- Civilization IV: Beyond the Sword (Steam, GOG or disc; patched to 3.19).

Nothing of the game is in this folder: it reads it from your install and writes nothing
into it.


Playing Beyond the Sword
------------------------
1. Unpack this folder anywhere.
2. Double-click "Play Beyond the Sword.bat": a menu opens where you start a new game (your
   leader, the map, its size, the difficulty, ...) or load a saved one.

To continue a saved game directly (the original's saves load too), drop it onto
"Play Beyond the Sword.bat".

The game autosaves every turn into Documents\My Games\openciv4\Saves\auto; the menu lists
those saves and the original's (Documents\My Games\Beyond the Sword\Saves).


Mods
----
Drop the mod's folder onto "Play Mod.bat", or run

    "Play Mod.bat" "Mods\My Mod"
    "Play Mod.bat" "D:\Mods\My Mod"

for a mod in Beyond the Sword\Mods or anywhere else.

Mods that change only XML, Python and art run on the game core in this folder. A mod with a
game core of its own runs when its Assets folder has the 64-bit build of it,
CvGameCoreDLL-x64.dll, beside the 32-bit CvGameCoreDLL.dll the original game loads. With only
the 32-bit one it does not run: the game core in this folder would read the mod's XML and fail.


Testing your port of a mod's game core
--------------------------------------
How to port a mod's game core to 64-bit, step by step (with a prompt for coding agents):

    https://github.com/bssth/openciv4-dll  (docs\PORTING.md)

To run your port:

1. Put your CvGameCoreDLL-x64.dll (and its .pdb, for readable crash reports) into the mod's
   Assets folder.
2. Check that it exports everything the engine needs, from a Visual Studio developer
   PowerShell (the engine checks it too when it starts and names what is missing):

       tools\check-exports.ps1 -Exe oc4core.exe -Dll "D:\Mods\My Mod\Assets\CvGameCoreDLL-x64.dll"

3. Run it: drop the mod's folder onto "Play Mod.bat". For the checks of the porting guide
   (XML loading, AI autoplay with checksums, saves) start oc4core.exe from a command prompt,
   e.g.

       oc4core.exe --mod "D:\Mods\My Mod" --map Continents --size SMALL --seed 3 --turns 20

   Every run prints what it does and which game core it loaded; a crash prints a stack trace
   with your DLL's functions.

Started from the menu once, each mod is remembered (Documents\My Games\openciv4\mods.txt):
the menu's "Load a Mod" switches to the others, and a save loads with its own mod.


Controls
--------
As in the original: click a unit to select it, right-click to send it, the game's hotkeys
for its actions, Enter ends the turn, the advisors and the Civilopedia from their buttons.
The mouse wheel zooms, dragging moves the map, Q/E (with Ctrl while playing) turn it, S saves
a screenshot.


Not there yet
-------------
The original's main menu (a simpler one stands in for it), the minimap's map modes, the globe
view, some animations (air and naval missions).


Third-party files in this folder
--------------------------------
- python27.dll and python\: Python 2.7.18, (c) Python Software Foundation, under the PSF
  License (python\LICENSE.txt).
- msvcp140.dll, vcruntime140*.dll and Microsoft.VC90.CRT\: the Microsoft Visual C++ runtime,
  redistributable.
- avcodec, avformat, avutil, swresample: FFmpeg, under the LGPL (FFmpeg-LICENSE.txt), for the
  movies.
- CvGameCoreDLL-x64.dll: the Civilization IV: Beyond the Sword SDK by Firaxis Games, compiled as
  64-bit (https://github.com/bssth/openciv4-dll).
