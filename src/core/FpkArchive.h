#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oc4
{
	// Firaxis pack file (Assets*.fpk). Layout (version 4):
	//   u32 version, "FPK_", u8 unknown, u32 fileCount,
	//   per file: u32 nameLen, name (each byte +1), pad to 4, u64 FILETIME, u32 size, u32 offset.
	// Data is stored uncompressed.
	class FpkArchive
	{
	public:
		struct Entry
		{
			std::string name; // as stored, backslashes
			uint64_t fileTime = 0;
			uint32_t size = 0;
			uint32_t offset = 0;
		};

		explicit FpkArchive(std::filesystem::path path);

		const std::filesystem::path& path() const { return m_path; }
		const std::vector<Entry>& entries() const { return m_entries; }

		// key: normalised asset key (lower case, forward slashes), see makeAssetKey.
		const Entry* find(std::string_view key) const;
		std::vector<std::byte> read(const Entry& entry) const;

	private:
		std::filesystem::path m_path;
		std::vector<Entry> m_entries;
		std::unordered_map<std::string, size_t> m_index;
	};

	// "Art\\Units\\X.nif" -> "art/units/x.nif"
	std::string makeAssetKey(std::string_view path);
}
