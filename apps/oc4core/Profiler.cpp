#include "Profiler.h"

#ifndef _WIN32

namespace oc4::host
{
	// (the sampling profiler walks x64 unwind tables; nothing samples here yet)
	void SamplingProfiler::start() {}
	void SamplingProfiler::stop() {}
	void SamplingProfiler::report(std::FILE* out, int) const { std::fputs("profiler: not available on this platform\n", out); }
	void SamplingProfiler::writeFolded(const std::filesystem::path&) const {}
}

#else

#include <dbghelp.h>
#include <timeapi.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace oc4::host
{
	namespace
	{
		constexpr int kMaxDepth = 128;

		// Walks a suspended thread's stack with the x64 unwind tables. No allocation here:
		// the thread may be suspended inside the heap. Faults end the walk.
		int unwind(CONTEXT ctx, DWORD64* frames)
		{
			int n = 0;
			__try
			{
				while (n < kMaxDepth && ctx.Rip)
				{
					frames[n++] = ctx.Rip;
					DWORD64 base = 0;
					PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
					if (!fn)
					{
						// leaf function: the return address is on top of the stack
						ctx.Rip = *reinterpret_cast<const DWORD64*>(ctx.Rsp);
						ctx.Rsp += 8;
					}
					else
					{
						PVOID handlerData = nullptr;
						DWORD64 establisher = 0;
						RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fn, &ctx, &handlerData, &establisher, nullptr);
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
			return n;
		}

		struct Symbols
		{
			HANDLE process = GetCurrentProcess();
			std::unordered_map<DWORD64, std::pair<std::string, std::string>> cache; // address -> (function, module)

			Symbols()
			{
				SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
				SymInitialize(process, nullptr, TRUE);
			}
			~Symbols() { SymCleanup(process); }

			const std::pair<std::string, std::string>& lookup(DWORD64 address)
			{
				auto [it, inserted] = cache.try_emplace(address);
				if (!inserted)
					return it->second;
				std::string module = "?";
				if (const DWORD64 base = SymGetModuleBase64(process, address))
				{
					char path[MAX_PATH] = {};
					GetModuleFileNameA(reinterpret_cast<HMODULE>(base), path, MAX_PATH);
					const char* slash = std::strrchr(path, '\\');
					module = slash ? slash + 1 : path;
				}
				alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512];
				auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = 511;
				DWORD64 displacement = 0;
				std::string function =
					SymFromAddr(process, address, &displacement, symbol) ? std::string(symbol->Name) : module + "+?";
				it->second = { module + "!" + function, module };
				return it->second;
			}
		};
	}

	void SamplingProfiler::start()
	{
		if (m_running)
			return;
		m_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
		m_frames.clear();
		m_offsets.assign(1, 0);
		m_running = true;
		m_thread = std::thread([this] { run(); });
	}

	void SamplingProfiler::stop()
	{
		if (!m_running)
			return;
		m_running = false;
		m_thread.join();
		CloseHandle(m_target);
		m_target = nullptr;
	}

	void SamplingProfiler::run()
	{
		timeBeginPeriod(1);
		DWORD64 frames[kMaxDepth];
		while (m_running.load(std::memory_order_relaxed))
		{
			Sleep(1);
			if (SuspendThread(m_target) == DWORD(-1))
				break;
			CONTEXT ctx{};
			ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
			const int n = GetThreadContext(m_target, &ctx) ? unwind(ctx, frames) : 0;
			ResumeThread(m_target);
			if (n > 0)
			{
				m_frames.insert(m_frames.end(), frames, frames + n);
				m_offsets.push_back(unsigned(m_frames.size()));
			}
		}
		timeEndPeriod(1);
	}

	void SamplingProfiler::report(std::FILE* out, int top) const
	{
		const size_t samples = m_offsets.size() - 1;
		if (samples == 0)
		{
			std::fprintf(out, "profile: no samples\n");
			return;
		}
		Symbols symbols;
		std::unordered_map<std::string, size_t> self, inclusive, modules;
		std::unordered_set<std::string> seen;
		for (size_t s = 0; s < samples; ++s)
		{
			const auto& [topName, topModule] = symbols.lookup(m_frames[m_offsets[s]]);
			++self[topName];
			++modules[topModule];
			seen.clear();
			for (unsigned f = m_offsets[s]; f < m_offsets[s + 1]; ++f)
			{
				const std::string& name = symbols.lookup(m_frames[f]).first;
				if (seen.insert(name).second)
					++inclusive[name];
			}
		}
		const auto print = [&](const char* title, const std::unordered_map<std::string, size_t>& counts, int n) {
			std::vector<std::pair<size_t, std::string>> sorted;
			for (const auto& [name, count] : counts)
				sorted.emplace_back(count, name);
			std::sort(sorted.rbegin(), sorted.rend());
			std::fprintf(out, "\n%s (%zu samples):\n", title, samples);
			for (int i = 0; i < n && i < int(sorted.size()); ++i)
				std::fprintf(out, "  %6.2f%%  %s\n", 100.0 * double(sorted[size_t(i)].first) / double(samples), sorted[size_t(i)].second.c_str());
		};
		print("self time by module", modules, 12);
		print("self time", self, top);
		print("inclusive time", inclusive, top);
	}

	void SamplingProfiler::writeFolded(const std::filesystem::path& path) const
	{
		Symbols symbols;
		std::map<std::string, size_t> stacks;
		for (size_t s = 0; s + 1 < m_offsets.size(); ++s)
		{
			std::string line;
			for (unsigned f = m_offsets[s + 1]; f-- > m_offsets[s];)
			{
				if (!line.empty())
					line += ';';
				line += symbols.lookup(m_frames[f]).first;
			}
			++stacks[line];
		}
		std::ofstream file(path);
		for (const auto& [stack, count] : stacks)
			file << stack << ' ' << count << '\n';
	}
}

#endif
