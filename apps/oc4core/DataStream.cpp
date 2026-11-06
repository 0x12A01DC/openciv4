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

	unsigned int MemoryStream::WriteString(const wchar* szName)
	{
		const int n = szName ? int(std::wcslen(szName)) : 0;
		Write(n);
		put(szName, n);
		return unsigned(n);
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
		Write(int(szName.size()));
		put(szName.data(), int(szName.size()));
		return unsigned(szName.size());
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
		const int n = readLength();
		get(szName, n);
		szName[n] = 0;
		return unsigned(n);
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
		const int n = readLength();
		szName.resize(size_t(n));
		get(szName.data(), n);
		return unsigned(n);
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
		const int n = readLength();
		auto* s = static_cast<wchar*>(std::malloc((size_t(n) + 1) * sizeof(wchar)));
		get(s, n);
		s[n] = 0;
		return s;
	}
}
