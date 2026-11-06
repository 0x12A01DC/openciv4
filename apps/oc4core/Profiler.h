#pragma once

// A sampling profiler for the host's own thread: a helper thread suspends it about once
// a millisecond, unwinds its stack (x64 unwind tables, so DLL and Python frames are seen
// too) and resumes it. The report lists functions by self time (top of the stack) and by
// inclusive time (anywhere on it), plus self time per module.

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

namespace oc4::host
{
	class SamplingProfiler
	{
	public:
		~SamplingProfiler() { stop(); }

		void start(); // samples the calling thread
		void stop();
		void report(std::FILE* out, int top) const;
		// Folded stacks ("outer;...;inner count" per line) for flame graph tools.
		void writeFolded(const std::filesystem::path& path) const;

	private:
		void run();

		std::thread m_thread;
		std::atomic<bool> m_running{ false };
		HANDLE m_target = nullptr;
		std::vector<DWORD64> m_frames;   // all samples, innermost frame first
		std::vector<unsigned> m_offsets; // sample i is m_frames[m_offsets[i] .. m_offsets[i + 1])
	};
}
