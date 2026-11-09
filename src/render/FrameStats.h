#pragma once

// The frame rate shown on screen: what the last half second or so did, the average and its
// longest frame. The figures stay until the next period ends, so they can be read.

#include <algorithm>
#include <cmath>

namespace oc4::render
{
	class FrameStats
	{
	public:
		// One frame, seconds after the last. Nonsense (a clock going back) counts for nothing.
		void add(double seconds)
		{
			if (!(seconds >= 0) || std::isinf(seconds))
				return;
			++m_frames;
			m_elapsed += seconds;
			m_worst = std::max(m_worst, seconds);
			if (m_elapsed < kPeriod)
				return;
			m_fps = double(m_frames) / m_elapsed;
			m_worstMs = m_worst * 1000;
			m_ready = true;
			m_frames = 0;
			m_elapsed = m_worst = 0;
		}

		bool ready() const { return m_ready; } // false: no period finished, nothing known yet
		double fps() const { return m_fps; }
		double worstMs() const { return m_worstMs; }

	private:
		static constexpr double kPeriod = 0.5;
		int m_frames = 0;
		double m_elapsed = 0, m_worst = 0, m_fps = 0, m_worstMs = 0;
		bool m_ready = false;
	};
}
