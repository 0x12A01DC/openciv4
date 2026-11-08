#pragma once

// Panning the map with W A S D, an option (Ctrl+Shift+W, the options' Game tab, --wasd-pan):
// which presses are the map's then and which still the game's. Only the letters alone pan; with
// Shift, Ctrl, Alt or Cmd they stay the game's hotkeys (Ctrl+S saves, Shift+D is diplomacy, Alt+S
// a sign), and with a menu, a screen or a popup up they are its letters (the arrows choose there,
// W A S D do not).

#include "render/Input.h"
#include "render/KeyNav.h"

#include <optional>

namespace oc4::render
{
	struct WasdState
	{
		bool on = false;     // the option
		bool menuUp = false; // a popup, a menu or a screen over the map
		bool shift = false, ctrl = false, alt = false, cmd = false;
	};

	// The way the letter pans the map, as the arrow it stands for; none for another key.
	inline std::optional<NavDirection> wasdDirection(unsigned k)
	{
		switch (k)
		{
		case 'W': return NavDirection::Up;
		case 'S': return NavDirection::Down;
		case 'A': return NavDirection::Left;
		case 'D': return NavDirection::Right;
		default: return std::nullopt;
		}
	}

	// The key is the map's now: it pans while held, and its press is not the game's hotkey.
	inline bool wasdPans(unsigned k, const WasdState& s)
	{
		return s.on && !s.menuUp && !s.shift && !s.ctrl && !s.alt && !s.cmd && wasdDirection(k).has_value();
	}

	// The option's own key, Ctrl+Shift+W (the game has Ctrl+W and Shift+W, not both).
	inline bool wasdToggle(unsigned k, bool shift, bool ctrl, bool alt) { return k == 'W' && shift && ctrl && !alt; }
}
