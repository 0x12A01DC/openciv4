openciv4 test build: Chronicles of Mankind
==========================================

A 64-bit engine for Civilization IV: Beyond the Sword. It runs the mod's own game core
(CvGameCoreDLL, rebuilt as 64-bit from the mod's source) and the game's and the mod's Python
scripts as they are, and does the rest itself: the map, the interface, the leaders, sound
and music.

This is a test build for the mod's author, not a release.


What you need
-------------
- Windows 10 or 11, 64-bit, with a Direct3D 12 graphics card.
- Civilization IV: Beyond the Sword (Steam, GOG or disc; patched to 3.19).
- Chronicles of Mankind installed. The game core in this folder is built from the source
  shared with the project (WIP of 2026-09-16), and was tested with CoM 1.3 + patch 1.3.4
  + the mod's Assets of September/October 2026. With another version of the mod's XML the
  game core may refuse to load it (it says so in openciv4.log).

Nothing of the game or the mod is in this folder: it reads them from your install, and
writes nothing into it.


How to play
-----------
1. Unpack this folder anywhere.
2. Double-click "Play CoM.bat": a menu opens where you start a new game (your leader, the
   map, its size, the difficulty, ...) or load a saved one.

It looks for the mod in "Beyond the Sword\Mods\Chronicles of Mankind". If yours is
elsewhere, drag its folder onto "Play CoM.bat", or give it:

    "Play CoM.bat" "D:\Mods\Chronicles of Mankind"

and to continue a saved game (the original's saves load too):

    "Play CoM.bat" "" "C:\...\Saves\single\MyGame.CivBeyondSwordSave"

The game autosaves every turn into Documents\My Games\openciv4\Saves\auto; the menu lists
those saves and the original's (Documents\My Games\Beyond the Sword\Saves).

Each run writes openciv4.log beside the launcher: please send it with any report, and a
save from just before a problem when there is one (the autosaves are fine).


Controls
--------
As in the original: click a unit to select it, right-click to send it, the game's hotkeys
for its actions, Enter ends the turn, the advisors and the Civilopedia from their buttons.
The mouse wheel zooms, dragging moves the map, the middle button turns it, S saves a
screenshot.


Not there yet
-------------
The original's main menu (a simpler one stands in for it), the wonders' Bink movies (the
wonder's model stands in), the globe view, World Builder, multiplayer.
Known errors of the mod itself, shown in the log: Assets\TechConquest.ini is missing
(Enhanced Tech Conquest), CultureLink's coordinates skip civilization 0 ("> 0" where
">= 0" is meant), FEATURE_BURNT_* are not in the feature infos.


Third-party files in this folder
--------------------------------
- python27.dll and python\: Python 2.7.18, (c) Python Software Foundation, under the PSF
  License (python\LICENSE.txt).
- msvcp140.dll, vcruntime140*.dll and Microsoft.VC90.CRT\: the Microsoft Visual C++
  runtime, redistributable.
- CvGameCoreDLL-x64.dll: the game core of Chronicles of Mankind (its authors' work, on
  AND2 and the Civilization IV SDK by Firaxis Games), compiled as 64-bit.
