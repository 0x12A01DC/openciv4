#pragma once

// How large the interface is drawn: one of its units is `scale` of the window's pixels. The
// scripts lay it out for the size this leaves, as for a screen of that resolution, and the map
// keeps every pixel. A window of more pixels than points (Retina) has that density by default,
// so the interface stays as large as the system's own text.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace oc4::render
{
	// The game's interface needs this much room.
	constexpr float kUiMinWidth = 1024, kUiMinHeight = 768;

	// percent: the player's choice, 0 for the window's density. Never so large that the
	// interface gets less room than it needs, never below 1.
	inline float uiScaleFor(int percent, float density, uint32_t width, uint32_t height)
	{
		const float wanted = percent > 0 ? float(percent) / 100 : density;
		const float scale = std::min({ wanted, float(width) / kUiMinWidth, float(height) / kUiMinHeight });
		return scale > 1 ? scale : 1; // (not std::max: a density of NaN is 1 too)
	}

	// The interface's whole units across that many pixels.
	inline int uiUnits(uint32_t pixels, float scale) { return int(std::floor(float(pixels) / scale + 1e-3f)); }
}
