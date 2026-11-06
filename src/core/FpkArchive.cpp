#include "core/FpkArchive.h"

#include "core/BinaryReader.h"
#include "core/FileUtil.h"

#include <array>
#include <format>
#include <fstream>
#include <stdexcept>

namespace oc4
{
	std::string makeAssetKey(std::string_view path)
	{
		std::string key;
		key.reserve(path.size());
		for (char c : path)
		{
			if (c == '\\')
				c = '/';
			else if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
			if (c == '/' && (key.empty() || key.back() == '/'))
				continue;
			key.push_back(c);
		}
		if (key.starts_with("./"))
			key.erase(0, 2);
		if (key.starts_with("assets/"))
			key.erase(0, 7);
		return key;
	}

	FpkArchive::FpkArchive(std::filesystem::path path) : m_path(std::move(path))
	{
		std::ifstream in(m_path, std::ios::binary);
		if (!in)
			throw std::runtime_error("cannot open " + m_path.string());

		// The directory sits at the front (~70 bytes per entry); 8 MB covers ~100k files.
		std::vector<std::byte> head(8 << 20);
		in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
		head.resize(static_cast<size_t>(in.gcount()));

		BinaryReader r(head);
		const uint32_t version = r.u32();
		const auto magic = r.read<std::array<char, 4>>();
		if (std::string_view(magic.data(), 4) != "FPK_")
			throw std::runtime_error(m_path.string() + ": not an FPK archive");
		if (version != 4)
			throw std::runtime_error(std::format("{}: unsupported FPK version {}", m_path.string(), version));
		(void)r.u8();
		const uint32_t count = r.u32();

		m_entries.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			Entry e;
			const uint32_t len = r.u32();
			if (len == 0 || len > 1024)
				r.fail(std::format("bad FPK name length {}", len));
			e.name.resize(len);
			for (uint32_t c = 0; c < len; ++c)
				e.name[c] = static_cast<char>(r.u8() - 1);
			if (len % 4 != 0)
				r.skip(4 - len % 4);
			e.fileTime = r.read<uint64_t>();
			e.size = r.u32();
			e.offset = r.u32();
			m_index.emplace(makeAssetKey(e.name), m_entries.size());
			m_entries.push_back(std::move(e));
		}
	}

	const FpkArchive::Entry* FpkArchive::find(std::string_view key) const
	{
		const auto it = m_index.find(std::string(key));
		return it == m_index.end() ? nullptr : &m_entries[it->second];
	}

	std::vector<std::byte> FpkArchive::read(const Entry& entry) const
	{
		std::ifstream in(m_path, std::ios::binary);
		in.seekg(entry.offset);
		std::vector<std::byte> data(entry.size);
		in.read(reinterpret_cast<char*>(data.data()), entry.size);
		if (static_cast<size_t>(in.gcount()) != entry.size)
			throw std::runtime_error(std::format("{}: short read of {}", m_path.string(), entry.name));
		return data;
	}
}
