#include "TurnWorker.h"

#include "PythonHost.h"

#include <windows.h>

#include <float.h>
#ifndef _WIN32
#include <pthread.h>
#endif

namespace oc4::host
{
	TurnWorker::TurnWorker()
	{
		// (OC4_SAME_THREAD=1: on the window's thread, as before)
		char value[8]{};
		if (GetEnvironmentVariableA("OC4_SAME_THREAD", value, sizeof(value)) > 0)
			return;
		// (the AI's path finding and recursion want room: as much stack as the window's thread has, and more)
#ifdef _WIN32
		m_thread = CreateThread(nullptr, 64u << 20, entry, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
#else
		pthread_attr_t attributes;
		pthread_attr_init(&attributes);
		pthread_attr_setstacksize(&attributes, 64u << 20);
		pthread_t thread{};
		if (pthread_create(&thread, &attributes, entry, this) == 0)
			m_thread = reinterpret_cast<void*>(thread);
		pthread_attr_destroy(&attributes);
#endif
	}

	TurnWorker::~TurnWorker()
	{
		if (m_running)
		{
			// (a job still running when the window closes: let it end first)
			while (!finish())
				Sleep(10);
		}
		{
			std::lock_guard lock(m_mutex);
			m_quit = true;
		}
		m_wake.notify_all();
		if (m_thread)
		{
#ifdef _WIN32
			WaitForSingleObject(m_thread, INFINITE);
			CloseHandle(m_thread);
#else
			pthread_join(reinterpret_cast<pthread_t>(m_thread), nullptr);
#endif
		}
	}

	void TurnWorker::start(std::function<void()> job)
	{
		if (m_running || !m_thread)
		{
			if (!m_thread)
				job(); // (no thread: here, as before)
			return;
		}
		_controlfp_s(&m_floatControl, 0, 0);
		m_python = PythonHost::releaseInterpreter();
		{
			std::lock_guard lock(m_mutex);
			m_job = std::move(job);
			m_done = false;
			m_error = nullptr;
		}
		m_running = true;
		m_wake.notify_all();
	}

	bool TurnWorker::finish()
	{
		if (!m_running)
			return true;
		{
			std::lock_guard lock(m_mutex);
			if (!m_done)
				return false;
		}
		PythonHost::takeInterpreter(m_python);
		m_python = nullptr;
		m_running = false;
		if (m_error)
			std::rethrow_exception(std::exchange(m_error, nullptr));
		return true;
	}

	bool TurnWorker::finishWithin(std::chrono::milliseconds wait)
	{
		if (!m_running)
			return true;
		{
			std::unique_lock lock(m_mutex);
			m_finished.wait_for(lock, wait, [&] { return m_done; });
		}
		return finish();
	}

#ifdef _WIN32
	unsigned long __stdcall TurnWorker::entry(void* self)
	{
		static_cast<TurnWorker*>(self)->loop();
		return 0;
	}
#else
	void* TurnWorker::entry(void* self)
	{
		static_cast<TurnWorker*>(self)->loop();
		return nullptr;
	}
#endif

	void TurnWorker::loop()
	{
		for (;;)
		{
			std::function<void()> job;
			{
				std::unique_lock lock(m_mutex);
				m_wake.wait(lock, [&] { return m_quit || (m_job && !m_done); });
				if (m_quit)
					return;
				job = m_job;
			}
			unsigned int unused = 0;
			_controlfp_s(&unused, m_floatControl, _MCW_DN | _MCW_EM | _MCW_RC);
			const int python = PythonHost::enterThread();
			try
			{
				job();
			}
			catch (...)
			{
				m_error = std::current_exception();
			}
			PythonHost::leaveThread(python);
			{
				std::lock_guard lock(m_mutex);
				m_job = nullptr;
				m_done = true;
			}
			m_finished.notify_all();
		}
	}
}
