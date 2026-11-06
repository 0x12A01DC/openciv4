#include "Unimplemented.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace oc4::host
{
	namespace
	{
		std::mutex g_mutex;
		std::map<std::string, size_t> g_calls;
	}

	void unimplemented(const char* what)
	{
		std::lock_guard lock(g_mutex);
		if (g_calls[what]++ == 0)
			std::fprintf(stderr, "[oc4core] unimplemented: %s\n", what);
	}

	void printUnimplementedSummary()
	{
		std::lock_guard lock(g_mutex);
		std::vector<std::pair<std::string, size_t>> sorted(g_calls.begin(), g_calls.end());
		std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.second > b.second; });
		if (sorted.empty())
		{
			std::fprintf(stderr, "[oc4core] no unimplemented interface methods were called\n");
			return;
		}
		std::fprintf(stderr, "[oc4core] %zu unimplemented interface methods were called:\n", sorted.size());
		for (const auto& [name, count] : sorted)
			std::fprintf(stderr, "  %8zu  %s\n", count, name.c_str());
	}
}
