// Checks the interface's scale (src/render/UiScale.h): what the player asks for, what the
// window's density gives, the room the game's interface needs, and windows too small for it.

#include "render/UiScale.h"

#include <cmath>
#include <cstdio>
#include <limits>

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

	bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }
}

int main()
{
	// A 13-inch Air's window: 1470 x 923 points, 2940 x 1846 pixels.
	check(near(uiScaleFor(0, 2, 2940, 1846), 2), "auto takes the Retina density");
	check(uiUnits(2940, 2) == 1470 && uiUnits(1846, 2) == 923, "auto lays out for the window's points");
	check(near(uiScaleFor(150, 2, 2940, 1846), 1.5f), "a chosen scale wins over the density");
	check(uiUnits(2940, 1.5f) == 1960 && uiUnits(1846, 1.5f) == 1230, "150% gives its units");
	check(near(uiScaleFor(100, 2, 2940, 1846), 1), "100% is one unit to the pixel");

	// A display without scaling, as on Windows: nothing changes.
	check(near(uiScaleFor(0, 1, 1600, 900), 1), "density 1 stays at 1");
	check(uiUnits(1600, 1) == 1600, "scale 1 keeps the pixels");

	// Not less room than the interface needs.
	check(near(uiScaleFor(300, 2, 2940, 1846), 1846 / 768.0f), "too large a choice stops at the height's room");
	check(uiUnits(1846, uiScaleFor(300, 2, 2940, 1846)) >= 768, "the height keeps 768 units");
	check(uiUnits(2048, uiScaleFor(0, 2, 2048, 1600)) >= 1024, "the width keeps 1024 units");
	check(near(uiScaleFor(0, 2, 2000, 1300), 2000 / 1024.0f) || near(uiScaleFor(0, 2, 2000, 1300), 1300 / 768.0f), "a small Retina window is limited");

	// A window smaller than that is not made smaller still.
	check(near(uiScaleFor(200, 2, 800, 600), 1), "never below 1");
	check(near(uiScaleFor(0, 0, 2940, 1846), 1), "a density of 0 is 1");
	check(near(uiScaleFor(0, std::numeric_limits<float>::quiet_NaN(), 2940, 1846), 1), "a density of NaN is 1");
	check(near(uiScaleFor(-5, 2, 2940, 1846), 2), "a bad choice falls back to the density");

	if (failures == 0)
		std::printf("ui scale: all passed\n");
	return failures != 0;
}
