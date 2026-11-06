#pragma once

// The game run off the window's thread: the other civilizations' turns take seconds in a big
// mod (one CvGame::update can be a whole AI player's turn), and the window would stand still
// meanwhile. One thread of its own runs a job (the game's updates) while the window's thread
// only draws what it captured before and takes the input; neither touches the game while the
// other does. Python goes with the job: the window's thread lets go of the interpreter for it
// and takes it back when it is done. The job runs with the window thread's floating point
// settings, so the game computes as it would there.

#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>

namespace oc4::host
{
	class TurnWorker
	{
	public:
		TurnWorker();
		~TurnWorker();
		TurnWorker(const TurnWorker&) = delete;
		TurnWorker& operator=(const TurnWorker&) = delete;

		// Starts the job (nothing may run): from here until finish() says it is done the game and
		// Python are the job's.
		void start(std::function<void()> job);
		bool running() const { return m_running; }
		// Whether the job is done (then the game and Python are the caller's again; what the job
		// threw is thrown here); false while it runs.
		bool finish();
		// finish(), waiting for the job at most this long.
		bool finishWithin(std::chrono::milliseconds wait);

	private:
		static unsigned long __stdcall entry(void* self);
		void loop();

		void* m_thread = nullptr;
		std::mutex m_mutex;
		std::condition_variable m_wake, m_finished;
		std::function<void()> m_job;
		bool m_quit = false, m_done = false, m_running = false;
		unsigned int m_floatControl = 0;
		void* m_python = nullptr; // the window thread's Python state while the job runs
		std::exception_ptr m_error;
	};
}
