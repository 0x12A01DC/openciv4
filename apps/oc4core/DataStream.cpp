#include "DataStream.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>

namespace oc4::host
{
	void MemoryStream::writeBytes(const void* p, size_t n)
	{
		if (m_pos + n > m_data.size())
			m_data.resize(m_pos + n);
		std::memcpy(m_data.data() + m_pos, p, n);
		m_pos += n;
	}

	void MemoryStream::readBytes(void* p, size_t n)
	{
		const size_t avail = m_pos < m_data.size() ? std::min(n, m_data.size() - m_pos) : 0;
		std::memcpy(p, m_data.data() + m_pos, avail);
		if (avail < n)
		{
			std::memset(static_cast<std::byte*>(p) + avail, 0, n - avail);
			m_overrun = true;
		}
		m_pos += n;
	}

	void MemoryStream::CopyToMem(void* mem)
	{
		if (!m_data.empty())
			std::memcpy(mem, m_data.data(), m_data.size());
	}

	int MemoryStream::readLength()
	{
		int n = 0;
		Read(&n);
		if (n < 0 || size_t(n) > GetSizeLeft())
		{
			m_overrun = true;
			return 0;
		}
		return n;
	}

	// ------------------------------------------------------------ strings

	unsigned int MemoryStream::writeUtf16(const wchar* s, size_t length)
	{
		std::u16string units;
		units.reserve(length);
		for (size_t i = 0; i < length; ++i)
		{
			const uint32_t c = static_cast<uint32_t>(s[i]);
			if (sizeof(wchar) == 4 && c >= 0x10000)
			{
				units.push_back(char16_t(0xD800 + ((c - 0x10000) >> 10)));
				units.push_back(char16_t(0xDC00 + ((c - 0x10000) & 0x3FF)));
			}
			else
				units.push_back(char16_t(c));
		}
		Write(int(units.size()));
		put(units.data(), int(units.size()));
		return unsigned(units.size());
	}

	std::wstring MemoryStream::readUtf16()
	{
		const int n = readLength();
		std::u16string units(size_t(n), u'\0');
		get(units.data(), n);
		std::wstring text;
		text.reserve(units.size());
		for (size_t i = 0; i < units.size(); ++i)
		{
			const char16_t c = units[i];
			if (sizeof(wchar) == 4 && c >= 0xD800 && c < 0xDC00 && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000)
				text.push_back(wchar(0x10000 + ((uint32_t(c) - 0xD800) << 10) + (uint32_t(units[++i]) - 0xDC00)));
			else
				text.push_back(wchar(c));
		}
		return text;
	}

	unsigned int MemoryStream::WriteString(const wchar* szName)
	{
		return writeUtf16(szName, szName ? std::wcslen(szName) : 0);
	}

	unsigned int MemoryStream::WriteString(const char* szName)
	{
		const int n = szName ? int(std::strlen(szName)) : 0;
		Write(n);
		put(szName, n);
		return unsigned(n);
	}

	unsigned int MemoryStream::WriteString(const std::string& szName)
	{
		Write(int(szName.size()));
		put(szName.data(), int(szName.size()));
		return unsigned(szName.size());
	}

	unsigned int MemoryStream::WriteString(const std::wstring& szName)
	{
		return writeUtf16(szName.data(), szName.size());
	}

	unsigned int MemoryStream::WriteString(int count, std::string values[])
	{
		unsigned int total = 0;
		for (int i = 0; i < count; ++i)
			total += WriteString(values[i]);
		return total;
	}

	unsigned int MemoryStream::WriteString(int count, std::wstring values[])
	{
		unsigned int total = 0;
		for (int i = 0; i < count; ++i)
			total += WriteString(values[i]);
		return total;
	}

	unsigned int MemoryStream::ReadString(char* szName)
	{
		const int n = readLength();
		get(szName, n);
		szName[n] = 0;
		return unsigned(n);
	}

	unsigned int MemoryStream::ReadString(wchar* szName)
	{
		const std::wstring text = readUtf16();
		std::copy(text.begin(), text.end(), szName);
		szName[text.size()] = 0;
		return unsigned(text.size());
	}

	unsigned int MemoryStream::ReadString(std::string& szName)
	{
		const int n = readLength();
		szName.resize(size_t(n));
		get(szName.data(), n);
		return unsigned(n);
	}

	unsigned int MemoryStream::ReadString(std::wstring& szName)
	{
		szName = readUtf16();
		return unsigned(szName.size());
	}

	unsigned int MemoryStream::ReadString(int count, std::string values[])
	{
		unsigned int total = 0;
		for (int i = 0; i < count; ++i)
			total += ReadString(values[i]);
		return total;
	}

	unsigned int MemoryStream::ReadString(int count, std::wstring values[])
	{
		unsigned int total = 0;
		for (int i = 0; i < count; ++i)
			total += ReadString(values[i]);
		return total;
	}

	// The DLL frees these with delete[], which its allocator routes to the engine's
	// free(). Never null: an empty string comes back as "" (CvString(NULL) would crash).
	char* MemoryStream::ReadString()
	{
		const int n = readLength();
		auto* s = static_cast<char*>(std::malloc(size_t(n) + 1));
		get(s, n);
		s[n] = 0;
		return s;
	}

	wchar* MemoryStream::ReadWideString()
	{
		const std::wstring text = readUtf16();
		auto* s = static_cast<wchar*>(std::malloc((text.size() + 1) * sizeof(wchar)));
		std::copy(text.begin(), text.end(), s);
		s[text.size()] = 0;
		return s;
	}
}
