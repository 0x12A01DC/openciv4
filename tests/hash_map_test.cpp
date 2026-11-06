// Checks the VC7.1-compatible stdext::hash_map (gamecore/compat/hash_map): iteration order
// against a key order read from a save of the original game, and against a direct model
// of its rules under random inserts and erases.

#include <hash_map>

#include <algorithm>
#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <vector>

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

	template <class Map>
	std::vector<int> keys(const Map& m)
	{
		std::vector<int> out;
		for (const auto& kv : m)
			out.push_back(kv.first);
		return out;
	}

	// The rules spelled out without the container's machinery.
	struct Model
	{
		std::uint32_t mask = 1, maxidx = 1, table = 8;
		std::vector<int> keys;

		std::uint32_t bucket(int k) const
		{
			std::uint32_t n = (static_cast<std::uint32_t>(k) ^ 0xDEADBEEFu) & mask;
			return maxidx <= n ? n - ((mask >> 1) + 1) : n;
		}
		void insert(int k)
		{
			if (maxidx <= keys.size() / 4)
			{
				if (table <= maxidx)
				{
					mask = (table << 1) - 1;
					table = mask + 1;
				}
				else if (mask < maxidx)
					mask = (mask << 1) + 1;
				++maxidx;
			}
			if (std::find(keys.begin(), keys.end(), k) == keys.end())
				keys.push_back(k);
		}
		void erase(int k) { keys.erase(std::remove(keys.begin(), keys.end(), k), keys.end()); }
		void clear() { *this = Model(); }
		std::vector<int> order() const
		{
			std::vector<int> out = keys;
			std::sort(out.begin(), out.end(), [&](int a, int b) { return bucket(a) != bucket(b) ? bucket(a) < bucket(b) : a < b; });
			return out;
		}
	};
}

int main()
{
	// Score history of a player in AutoSave_BC-3520 (BtS 3.19 hotseat), keys = turns 0..16.
	{
		stdext::hash_map<int, int> m;
		for (int turn = 0; turn <= 16; ++turn)
			m[turn] = turn * 10;
		check(keys(m) == std::vector<int>{ 7, 15, 2, 6, 10, 14, 1, 5, 9, 13, 0, 4, 8, 12, 16, 3, 11 }, "order of an original save");
	}

	// Random inserts, erases and clears against the model; finds and stable references.
	std::mt19937 rng(12345);
	for (int round = 0; round < 200; ++round)
	{
		stdext::hash_map<int, int> m;
		Model model;
		std::map<int, const int*> addresses;
		const int range = 1 + int(rng() % 400);
		for (int op = 0; op < 2000; ++op)
		{
			const int k = int(rng() % unsigned(range)) - range / 4; // negative keys too
			const unsigned what = rng() % 100;
			if (what < 60)
			{
				m.insert(std::make_pair(k, k * 3));
				model.insert(k);
				addresses.emplace(k, &m.find(k)->second);
			}
			else if (what < 95)
			{
				m.erase(k);
				model.erase(k);
				addresses.erase(k);
			}
			else if (what == 99)
			{
				m.clear();
				model.clear();
				addresses.clear();
			}
		}
		check(keys(m) == model.order(), "order after random operations");
		bool found = true, stable = true;
		for (const auto& [k, p] : addresses)
		{
			const auto it = m.find(k);
			found = found && it != m.end() && it->second == k * 3;
			stable = stable && it != m.end() && &it->second == p;
		}
		check(found, "find after random operations");
		check(stable, "element addresses stay valid");
		check(m.find(range * 10) == m.end(), "find of a missing key");

		const stdext::hash_map<int, int> copy = m;
		check(keys(copy) == keys(m), "copy keeps the order");
		stdext::hash_map<int, int> moved = std::move(stdext::hash_map<int, int>(m));
		check(keys(moved) == keys(m), "move keeps the order");
		moved.insert(std::make_pair(range * 10, 1));
		model.insert(range * 10);
		check(keys(moved) == model.order(), "insert after move");
	}

	// String keys, looked up by std::string and by C string.
	{
		stdext::hash_map<std::string, int> m;
		for (int i = 0; i < 500; ++i)
			m["TYPE_" + std::to_string(i * 7)] = i;
		bool ok = true;
		for (int i = 0; i < 500; ++i)
		{
			const std::string key = "TYPE_" + std::to_string(i * 7);
			ok = ok && m.find(key) != m.end() && m.find(key.c_str()) == m.find(key) && m.find(key.c_str())->second == i;
		}
		const auto& cm = m;
		ok = ok && m.find("TYPE_1") == m.end() && cm.find("TYPE_7") != cm.end() && cm.find("") == cm.end();
		check(ok, "string keys");
	}

	// Erasing while iterating, as CvPlayer::doEvents does.
	{
		stdext::hash_map<int, int> m;
		for (int i = 0; i < 100; ++i)
			m[i] = i;
		for (auto it = m.begin(); it != m.end();)
			it = (it->first % 3 == 0) ? m.erase(it) : std::next(it);
		bool ok = m.size() == 66;
		for (int i = 0; i < 100; ++i)
			ok = ok && ((m.find(i) != m.end()) == (i % 3 != 0));
		check(ok, "erase while iterating");
	}

	std::printf(failures ? "%d check(s) failed\n" : "all hash_map checks passed\n", failures);
	return failures ? 1 : 0;
}
