#include "core/BinaryReader.h"

#include <format>

namespace oc4
{
	std::string BinaryReader::readLine(size_t maxLen)
	{
		std::string out;
		while (true)
		{
			const auto c = static_cast<char>(u8());
			if (c == '\n')
				return out;
			if (out.size() >= maxLen)
				fail("line too long");
			out.push_back(c);
		}
	}

	std::string BinaryReader::readSizedString(size_t maxLen)
	{
		const uint32_t len = u32();
		if (len > maxLen)
			fail(std::format("string length {} too large", len));
		require(len);
		std::string out(reinterpret_cast<const char*>(m_data.data() + m_pos), len);
		m_pos += len;
		return out;
	}

	void BinaryReader::fail(std::string_view what) const
	{
		throw ParseError(std::format("{} at offset 0x{:X}", what, m_pos), m_pos);
	}
}
