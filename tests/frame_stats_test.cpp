// Checks the on-screen frame rate (src/render/FrameStats.h): nothing claimed before a period
// ends, the average and the longest frame of a period, one very long frame, and bad clocks.

#include "render/FrameStats.h"

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

	bool near(double a, double b, double by = 1e-6) { return std::abs(a - b) < by; }
}

int main()
{
	{
		FrameStats s;
		check(!s.ready(), "nothing known at first");
		s.add(0.1);
		check(!s.ready(), "nothing known before a period ends");
		check(s.fps() == 0 && s.worstMs() == 0, "no figures before a period ends");
	}
	{
		FrameStats s;
		for (int i = 0; i < 31; ++i)
			s.add(1.0 / 60);
		check(s.ready(), "half a second of frames ends a period");
		check(near(s.fps(), 60, 1e-3), "steady frames give their rate");
		check(near(s.worstMs(), 1000.0 / 60, 1e-3), "steady frames are their own worst");
	}
	{
		FrameStats s;
		for (int i = 0; i < 20; ++i)
			s.add(0.01);
		s.add(0.3); // a stutter
		check(s.ready(), "a stutter ends the period");
		check(near(s.worstMs(), 300), "the longest frame is reported");
		check(near(s.fps(), 21 / 0.5, 1e-6), "the average counts every frame");
	}
	{
		FrameStats s;
		s.add(2);
		check(s.ready() && near(s.fps(), 0.5) && near(s.worstMs(), 2000), "one very long frame is not capped");
		for (int i = 0; i < 30; ++i)
			s.add(0.001);
		check(near(s.fps(), 0.5) && near(s.worstMs(), 2000), "the figures stay until the next period ends");
		for (int i = 0; i < 480; ++i)
			s.add(0.001);
		check(near(s.worstMs(), 1, 1e-6), "the next period forgets the last one's worst");
	}
	{
		FrameStats s;
		s.add(-1);
		s.add(std::numeric_limits<double>::quiet_NaN());
		s.add(std::numeric_limits<double>::infinity());
		check(!s.ready(), "bad times count for nothing");
		s.add(0);
		check(!s.ready(), "a frame of no time ends nothing");
	}
	if (failures == 0)
		std::printf("frame stats: all passed\n");
	return failures != 0;
}
