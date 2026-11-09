#pragma once

// Choosing with the keyboard: the next stop of a list, and the nearest thing in a direction on
// a screen laid out anyhow (the scripts place their widgets freely, so there is no tab order).

#include <algorithm>
#include <cmath>
#include <vector>

namespace oc4::render
{
	enum class NavDirection
	{
		Up, Down, Left, Right
	};

	struct NavRect
	{
		float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		float cx() const { return (x0 + x1) / 2; }
		float cy() const { return (y0 + y1) / 2; }
	};

	// The next of `count` stops from `at` (below 0: none yet), round the ends. -1 with no stops.
	inline int navStep(int count, int at, int delta)
	{
		if (count <= 0)
			return -1;
		if (at < 0 || at >= count)
			return delta > 0 ? 0 : count - 1;
		return (at + delta % count + count) % count;
	}

	namespace nav_detail
	{
		inline bool vertical(NavDirection d) { return d == NavDirection::Up || d == NavDirection::Down; }

		// How far apart two spans are across the direction of travel, 0 when they overlap.
		inline float gap(float a0, float a1, float b0, float b1) { return std::max({ 0.0f, b0 - a1, a0 - b1 }); }

		// Moving on from `a` to `b`: how far along the direction, negative behind it.
		inline float ahead(const NavRect& a, const NavRect& b, NavDirection d)
		{
			switch (d)
			{
			case NavDirection::Up: return a.cy() - b.cy();
			case NavDirection::Down: return b.cy() - a.cy();
			case NavDirection::Left: return a.cx() - b.cx();
			default: return b.cx() - a.cx();
			}
		}

		inline float across(const NavRect& a, const NavRect& b, NavDirection d)
		{
			return vertical(d) ? gap(a.x0, a.x1, b.x0, b.x1) + std::abs(a.cx() - b.cx()) * 0.25f :
				gap(a.y0, a.y1, b.y0, b.y1) + std::abs(a.cy() - b.cy()) * 0.25f;
		}
	}

	// The nearest rectangle from rects[from] that way, what lines up with it first; -1 none.
	// With no `from`, the first one coming in from that side's far end.
	inline int navNearest(const std::vector<NavRect>& rects, int from, NavDirection d)
	{
		constexpr float kSameLine = 1;
		int best = -1;
		float bestScore = 0;
		for (int i = 0; i < int(rects.size()); ++i)
		{
			float score;
			if (from < 0 || from >= int(rects.size()))
			{
				const bool forward = d == NavDirection::Down || d == NavDirection::Right;
				const float along = nav_detail::vertical(d) ? rects[size_t(i)].cy() : rects[size_t(i)].cx();
				const float side = nav_detail::vertical(d) ? rects[size_t(i)].cx() : rects[size_t(i)].cy();
				score = (forward ? along : -along) * 4 + (forward ? side : -side);
			}
			else
			{
				if (i == from)
					continue;
				const float forward = nav_detail::ahead(rects[size_t(from)], rects[size_t(i)], d);
				if (forward < kSameLine)
					continue;
				score = forward + nav_detail::across(rects[size_t(from)], rects[size_t(i)], d) * 3;
			}
			if (best < 0 || score < bestScore)
				best = i, bestScore = score;
		}
		return best;
	}

	// Nothing further that way: round to the far side, what lines up first; -1 none other.
	inline int navWrap(const std::vector<NavRect>& rects, int from, NavDirection d)
	{
		if (from < 0 || from >= int(rects.size()))
			return navNearest(rects, -1, d);
		int best = -1;
		float bestScore = 0;
		for (int i = 0; i < int(rects.size()); ++i)
		{
			if (i == from)
				continue;
			// (the furthest behind: `ahead` is negative for those)
			const float score = nav_detail::ahead(rects[size_t(from)], rects[size_t(i)], d) + nav_detail::across(rects[size_t(from)], rects[size_t(i)], d) * 3;
			if (best < 0 || score < bestScore)
				best = i, bestScore = score;
		}
		return best;
	}
}
