// Checks the globe view's bend on the CPU (src/render/GlobeBend.h), which places the overlay's
// marks (a globe layer's colours) where the globe shows the map: flat when there is no globe, the
// centre unmoved, a quarter of the way round on the sphere's side, the wrap, heights outward, and
// the far side hidden from the eye.

#include "render/GlobeBend.h"

#include <cmath>
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

	bool near(float a, float b) { return std::abs(a - b) < 1e-2f; }
	bool near(GlobePoint a, GlobePoint b) { return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z); }
}

int main()
{
	const float width = 6283.1853f;                 // a map once round a sphere of radius 1000
	const GlobeShape globe{ 500, 300, 1000, width };
	const GlobeShape flat{};

	// No globe: the flat map's point.
	check(near(globeBend(flat, { 12, 34, 5 }), { 12, 34, 5 }), "flat map unchanged");
	check(globeFacing(flat, { 12, 34, 5 }, { 0, 0, -100 }), "flat map always faces");

	// The point looked at stays put, its height straight up.
	check(near(globeBend(globe, { 500, 300, 0 }), { 500, 300, 0 }), "centre unmoved");
	check(near(globeBend(globe, { 500, 300, 7 }), { 500, 300, 7 }), "centre's height straight up");

	// A quarter of the way round (pi/2 * radius east): on the sphere's side, level with its centre.
	const float quarter = 3.14159265f / 2 * 1000;
	check(near(globeBend(globe, { 500 + quarter, 300, 0 }), { 1500, 300, -1000 }), "quarter east on the side");
	check(near(globeBend(globe, { 500, 300 - quarter, 0 }), { 500, -700, -1000 }), "quarter south on the side");
	// (a height there points outward, away from the centre)
	check(near(globeBend(globe, { 500 + quarter, 300, 10 }), { 1510, 300, -1000 }), "height outward on the side");

	// The wrap: a point a whole map width away is the same place.
	check(near(globeBend(globe, { 500 + quarter + width, 300, 0 }), globeBend(globe, { 500 + quarter, 300, 0 })), "wrapped copy same place");
	check(near(globeBend(globe, { 500 - quarter - width, 300, 0 }), globeBend(globe, { 500 - quarter, 300, 0 })), "wrapped copy west same place");
	// (without wrap, the far point is not folded back)
	const GlobeShape noWrap{ 500, 300, 1000, 0 };
	check(!near(globeBend(noWrap, { 500 + quarter + width, 300, 0 }), globeBend(noWrap, { 500 + quarter, 300, 0 })), "no wrap: not folded");

	// Facing: the eye straight above the centre sees the near cap, not the far side.
	const GlobePoint eye{ 500, 300, 3000 };
	check(globeFacing(globe, globeBend(globe, { 500, 300, 0 }), eye), "centre faces the eye");
	check(globeFacing(globe, globeBend(globe, { 500 + 400, 300 + 300, 0 }), eye), "near point faces the eye");
	check(!globeFacing(globe, globeBend(globe, { 500 + 2 * quarter * 0.9f, 300, 0 }), eye), "far side hidden");
	check(!globeFacing(globe, globeBend(globe, { 500 + quarter, 300, 0 }), eye), "limb at a quarter hidden from a finite eye");

	if (failures == 0)
		std::printf("globe_bend: all checks passed\n");
	return failures == 0 ? 0 : 1;
}
