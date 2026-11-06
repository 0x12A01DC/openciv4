openciv4 demo: Rise of Mankind - A New Dawn 2
=============================================

A 64-bit engine for Civilization IV: Beyond the Sword. It runs the mod's own game core
(CvGameCoreDLL, rebuilt as 64-bit) and the game's Python scripts as they are, and does
the rest itself: the map, the interface, the leaders, sound and music.

This is a work in progress demo, not a release.


What you need
-------------
- Windows 10 or 11, 64-bit, with a Direct3D 12 graphics card.
- Civilization IV: Beyond the Sword (Steam, GOG or disc; patched to 3.19).
- Rise of Mankind - A New Dawn 2 at SVN revision 1091. The game core in this folder is
  built from that revision: another version of the mod (or a mod built on it, with its
  own changes to the game core) may not load.

Nothing of the game or the mod is in this folder: it reads them from your install.


How to play
-----------
1. Unpack this folder anywhere.
2. Double-click "Play AND2.bat": a menu opens where you start a new game (your leader,
   the map, its size, the difficulty, ...) or load a saved one.

It looks for the mod in "Beyond the Sword\Mods\Rise of Mankind - A New Dawn". If yours is
elsewhere, drag its folder onto "Play AND2.bat", or give it:

    "Play AND2.bat" "D:\Mods\Rise of Mankind - A New Dawn"

and to continue a saved game (the original's saves load too):

    "Play AND2.bat" "" "C:\...\Saves\single\MyGame.CivBeyondSwordSave"

The game autosaves every turn into Documents\My Games\openciv4\Saves\auto; the menu
lists those saves and the original's (Documents\My Games\Beyond the Sword\Saves).


Controls
--------
As in the original: click a unit to select it, right-click to send it, the game's hotkeys
for its actions, Enter ends the turn, the advisors and the Civilopedia from their
buttons. The mouse wheel zooms, dragging moves the map, Q/E (with Ctrl while playing)
turn it, S saves a screenshot.


Not there yet
-------------
The original's main menu (a simpler one stands in for it), the minimap's map modes, the globe view,
multiplayer, some animations (air and naval missions), the leaders' full shaders,
options for volume and graphics.


Third-party files in this folder
--------------------------------
- python27.dll and python\: Python 2.7.18, (c) Python Software Foundation, under the
  PSF License (python\LICENSE.txt).
- msvcp140.dll, vcruntime140*.dll and Microsoft.VC90.CRT\: the Microsoft Visual C++
  runtime, redistributable.
- CvGameCoreDLL-x64.dll: the game core of Rise of Mankind - A New Dawn 2 (its authors' work,
  on the Civilization IV SDK by Firaxis Games), compiled as 64-bit.
