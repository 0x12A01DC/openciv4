// Checks W A S D panning's routing (src/render/WasdPan.h): off, the letters are the game's
// hotkeys; on, the letters alone are the map's, with a modifier or under a menu still the game's.

#include "render/WasdPan.h"

#include <cstdio>

using namespace oc4::render;

namespace
{
	int failures = 0;

	void check(bool ok, const char* what)
	{
		if (!ok)
		{
			std::printf("FAILED: %s\n", what);
			++failures;
		}
	}
}

int main()
{
	check(wasdDirection('W') == NavDirection::Up, "W is up");
	check(wasdDirection('S') == NavDirection::Down, "S is down");
	check(wasdDirection('A') == NavDirection::Left, "A is left");
	check(wasdDirection('D') == NavDirection::Right, "D is right");
	check(!wasdDirection('Q') && !wasdDirection('E') && !wasdDirection(key::Up) && !wasdDirection(0), "no other key pans");
	check(!wasdDirection('w'), "lower case is no key code");

	WasdState off{};
	for (const unsigned k : { 'W', 'A', 'S', 'D' })
		check(!wasdPans(k, off), "off: the letters are the game's");

	WasdState on{ .on = true };
	for (const unsigned k : { 'W', 'A', 'S', 'D' })
		check(wasdPans(k, on), "on: the letters alone are the map's");
	check(!wasdPans('F', on) && !wasdPans(key::Space, on) && !wasdPans(key::Up, on), "on: other keys stay the game's");

	// The game's own shortcuts on those letters.
	check(!wasdPans('S', { .on = true, .ctrl = true }), "Ctrl+S saves");
	check(!wasdPans('D', { .on = true, .shift = true }), "Shift+D is diplomacy");
	check(!wasdPans('S', { .on = true, .alt = true }), "Alt+S is a sign");
	check(!wasdPans('W', { .on = true, .shift = true }), "Shift+W builds");
	check(!wasdPans('W', { .on = true, .cmd = true }), "Cmd+W is not panning");

	// A menu, screen or popup over the map has its letters.
	check(!wasdPans('W', { .on = true, .menuUp = true }), "under a menu the letters are its");

	check(wasdToggle('W', true, true, false, true), "on: Ctrl+Shift+W turns it off");
	check(!wasdToggle('W', true, true, false, false), "off: Ctrl+Shift+W is the game's (show a wonder)");
	check(!wasdToggle('W', false, true, false, true), "Ctrl+W is the world builder");
	check(!wasdToggle('W', true, false, false, true), "Shift+W builds");
	check(!wasdToggle('W', true, true, true, true), "not with Alt too");
	check(!wasdToggle('F', true, true, false, true), "Ctrl+Shift+F is the frame rate");

	if (failures == 0)
		std::printf("wasd_pan_test: all passed\n");
	return failures == 0 ? 0 : 1;
}
