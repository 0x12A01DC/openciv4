// Checks the keyboard's choosing (src/render/KeyNav.h): a list's stops, and moving between
// widgets laid out in columns, rows and untidy places, with nothing there and at the edges.

#include "render/KeyNav.h"

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

	NavRect box(float x, float y, float w = 100, float h = 30) { return { x, y, x + w, y + h }; }
}

int main()
{
	check(navStep(0, -1, 1) == -1, "no stops, nothing chosen");
	check(navStep(3, -1, 1) == 0, "down from nothing is the first");
	check(navStep(3, -1, -1) == 2, "up from nothing is the last");
	check(navStep(3, 0, 1) == 1, "down one");
	check(navStep(3, 2, 1) == 0, "down from the last goes round");
	check(navStep(3, 0, -1) == 2, "up from the first goes round");
	check(navStep(1, 0, 1) == 0, "one stop stays");
	check(navStep(3, 7, 1) == 0, "a stale place starts again");

	// A column, as the menus.
	const std::vector<NavRect> column{ box(100, 100), box(100, 150), box(100, 200) };
	check(navNearest(column, 0, NavDirection::Down) == 1, "the next one down");
	check(navNearest(column, 2, NavDirection::Up) == 1, "the next one up");
	check(navNearest(column, 2, NavDirection::Down) == -1, "nothing below the last");
	check(navNearest(column, 1, NavDirection::Left) == -1, "nothing beside a column");
	check(navNearest(column, -1, NavDirection::Down) == 0, "coming in from above");
	check(navNearest(column, -1, NavDirection::Up) == 2, "coming in from below");
	check(navWrap(column, 2, NavDirection::Down) == 0, "round to the top");
	check(navWrap(column, 0, NavDirection::Up) == 2, "round to the bottom");

	// Two columns: down keeps to its own, across keeps to its row.
	const std::vector<NavRect> grid{ box(100, 100), box(300, 100), box(100, 150), box(300, 150) };
	check(navNearest(grid, 0, NavDirection::Down) == 2, "down keeps to the left column");
	check(navNearest(grid, 1, NavDirection::Down) == 3, "down keeps to the right column");
	check(navNearest(grid, 0, NavDirection::Right) == 1, "right keeps to the row");
	check(navNearest(grid, 3, NavDirection::Left) == 2, "left keeps to the row");
	check(navWrap(grid, 2, NavDirection::Down) == 0, "wrapping keeps to the column");

	// A wide button under narrow ones: what lines up beats what is a little nearer.
	const std::vector<NavRect> untidy{ box(100, 100), box(400, 120), box(100, 170, 300) };
	check(navNearest(untidy, 0, NavDirection::Down) == 2, "lines up rather than nearer but aside");

	check(navNearest({}, -1, NavDirection::Down) == -1, "an empty screen");
	check(navNearest({ box(0, 0) }, 0, NavDirection::Down) == -1, "alone on the screen");
	check(navWrap({ box(0, 0) }, 0, NavDirection::Down) == -1, "alone, nowhere to go round to");
	check(navNearest(column, 9, NavDirection::Down) == 0, "a stale place starts again");

	if (failures == 0)
		std::printf("key nav: all passed\n");
	return failures != 0;
}
