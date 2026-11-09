#pragma once

// The globe view's bend on the CPU: the same as the shaders' globeBend (Renderer.h), so what the
// overlay draws over the map (a globe layer's colours, borders, signs) lands where the globe
// shows those places.

#include <cmath>

namespace oc4::render
{
	struct GlobeShape
	{
		float x = 0, y = 0;  // the point it is centred on
		float radius = 0;    // 0: flat
		float wrapWidth = 0; // 0: not wrapping
	};

	struct GlobePoint
	{
		float x = 0, y = 0, z = 0;
	};

	// A point of the flat map (z above the ground's plane) where the globe puts it.
	inline GlobePoint globeBend(const GlobeShape& g, GlobePoint p)
	{
		if (g.radius <= 0)
			return p;
		float dx = p.x - g.x, dy = p.y - g.y;
		if (g.wrapWidth > 0)
			dx -= g.wrapWidth * std::round(dx / g.wrapWidth);
		const float len = std::sqrt(dx * dx + dy * dy);
		const float a = std::fmin(len / g.radius, 3.14159f);
		const float ux = len > 1e-4f ? dx / len : 0, uy = len > 1e-4f ? dy / len : 0;
		const float h = g.radius + p.z;
		return { g.x + ux * h * std::sin(a), g.y + uy * h * std::sin(a), h * std::cos(a) - g.radius };
	}

	// Whether a bent point faces the eye (not round the far side of the sphere).
	inline bool globeFacing(const GlobeShape& g, GlobePoint bent, GlobePoint eye)
	{
		if (g.radius <= 0)
			return true;
		const float nx = bent.x - g.x, ny = bent.y - g.y, nz = bent.z + g.radius; // (from the sphere's centre)
		return nx * (eye.x - bent.x) + ny * (eye.y - bent.y) + nz * (eye.z - bent.z) > 0;
	}
}
