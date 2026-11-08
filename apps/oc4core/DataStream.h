#pragma once

// The engine's FDataStreamBase over a memory buffer: what the DLL serialises the game
// through. Little-endian, ints/longs 4 bytes (whatever `long` is on this platform), bool 1 byte,
// strings as an int character count followed by the characters (wide: UTF-16 units, whatever
// wchar_t is), without terminator; exactly the layout of the original 32-bit engine's saves.
//
// Reading past the end yields zeros and sets overrun() instead of throwing through the
// DLL; the save loader checks it.

#include "CvGameCoreDLL.h"
#include "FDataStreamBase.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace oc4::host
{
	class MemoryStream : public FDataStreamBase
	{
	public:
		MemoryStream() = default;
		explicit MemoryStream(std::vector<std::byte> data) : m_data(std::move(data)) {}

		const std::vector<std::byte>& data() const { return m_data; }
		std::vector<std::byte>& data() { return m_data; }
		bool overrun() const { return m_overrun; }

		void Rewind() override { m_pos = 0; }
		bool AtEnd() override { return m_pos >= m_data.size(); }
		void FastFwd() override { m_pos = m_data.size(); }
		unsigned int GetPosition() const override { return static_cast<unsigned int>(m_pos); }
		void SetPosition(unsigned int position) override { m_pos = position; }
		void Truncate() override { m_data.resize(std::min(m_pos, m_data.size())); }
		void Flush() override {}
		unsigned int GetEOF() const override { return static_cast<unsigned int>(m_data.size()); }
		unsigned int GetSizeLeft() const override { return static_cast<unsigned int>(m_pos < m_data.size() ? m_data.size() - m_pos : 0); }
		void CopyToMem(void* mem) override;

		unsigned int WriteString(const wchar* szName) override;
		unsigned int WriteString(const char* szName) override;
		unsigned int WriteString(const std::string& szName) override;
		unsigned int WriteString(const std::wstring& szName) override;
		unsigned int WriteString(int count, std::string values[]) override;
		unsigned int WriteString(int count, std::wstring values[]) override;

		unsigned int ReadString(char* szName) override;
		unsigned int ReadString(wchar* szName) override;
		unsigned int ReadString(std::string& szName) override;
		unsigned int ReadString(std::wstring& szName) override;
		unsigned int ReadString(int count, std::string values[]) override;
		unsigned int ReadString(int count, std::wstring values[]) override;
		char* ReadString() override;
		wchar* ReadWideString() override;

		void Read(char* v) override { get(v, 1); }
		void Read(byte* v) override { get(v, 1); }
		void Read(int count, char values[]) override { get(values, count); }
		void Read(int count, byte values[]) override { get(values, count); }
		void Read(bool* v) override { get(v, 1); }
		void Read(int count, bool values[]) override { get(values, count); }
		void Read(short* s) override { get(s, 1); }
		void Read(unsigned short* s) override { get(s, 1); }
		void Read(int count, short values[]) override { get(values, count); }
		void Read(int count, unsigned short values[]) override { get(values, count); }
		void Read(int* i) override { get(i, 1); }
		void Read(unsigned int* i) override { get(i, 1); }
		void Read(int count, int values[]) override { get(values, count); }
		void Read(int count, unsigned int values[]) override { get(values, count); }
		void Read(long* l) override { getWide32(l, 1); }
		void Read(unsigned long* l) override { getWide32(l, 1); }
		void Read(int count, long values[]) override { getWide32(values, count); }
		void Read(int count, unsigned long values[]) override { getWide32(values, count); }
		void Read(float* value) override { get(value, 1); }
		void Read(int count, float values[]) override { get(values, count); }
		void Read(double* value) override { get(value, 1); }
		void Read(int count, double values[]) override { get(values, count); }

		void Write(char value) override { put(&value, 1); }
		void Write(byte value) override { put(&value, 1); }
		void Write(int count, const char values[]) override { put(values, count); }
		void Write(int count, const byte values[]) override { put(values, count); }
		void Write(bool value) override { put(&value, 1); }
		void Write(int count, const bool values[]) override { put(values, count); }
		void Write(short value) override { put(&value, 1); }
		void Write(unsigned short value) override { put(&value, 1); }
		void Write(int count, const short values[]) override { put(values, count); }
		void Write(int count, const unsigned short values[]) override { put(values, count); }
		void Write(int value) override { put(&value, 1); }
		void Write(unsigned int value) override { put(&value, 1); }
		void Write(int count, const int values[]) override { put(values, count); }
		void Write(int count, const unsigned int values[]) override { put(values, count); }
		void Write(long value) override { putWide32(&value, 1); }
		void Write(unsigned long value) override { putWide32(&value, 1); }
		void Write(int count, const long values[]) override { putWide32(values, count); }
		void Write(int count, const unsigned long values[]) override { putWide32(values, count); }
		void Write(float value) override { put(&value, 1); }
		void Write(int count, const float values[]) override { put(values, count); }
		void Write(double value) override { put(&value, 1); }
		void Write(int count, const double values[]) override { put(values, count); }
		using FDataStreamBase::Write;

		// Raw bytes (the save container around the DLL's data).
		void writeBytes(const void* p, size_t n);
		void readBytes(void* p, size_t n);

	private:
		static_assert(sizeof(bool) == 1 && sizeof(short) == 2 && sizeof(int) == 4 && sizeof(float) == 4 && sizeof(double) == 8, "save layout");
		static_assert(sizeof(long) == 4 || sizeof(long) == 8);
		static_assert(sizeof(wchar) == 2 || sizeof(wchar) == 4);

		template <class T>
		void put(const T* values, int count)
		{
			if (count > 0)
				writeBytes(values, sizeof(T) * size_t(count));
		}
		template <class T>
		void get(T* values, int count)
		{
			if (count > 0)
				readBytes(values, sizeof(T) * size_t(count));
		}
		// A `long` is 4 bytes in a save, whatever it is here.
		template <class T>
		void putWide32(const T* values, int count)
		{
			for (int i = 0; i < count; ++i)
			{
				const std::conditional_t<std::is_signed_v<T>, int32_t, uint32_t> v = static_cast<decltype(v + 0)>(values[i]);
				put(&v, 1);
			}
		}
		template <class T>
		void getWide32(T* values, int count)
		{
			for (int i = 0; i < count; ++i)
			{
				std::conditional_t<std::is_signed_v<T>, int32_t, uint32_t> v = 0;
				get(&v, 1);
				values[i] = v;
			}
		}
		// Wide strings: the count is of UTF-16 units, as on Windows.
		unsigned int writeUtf16(const wchar* s, size_t length);
		std::wstring readUtf16();
		int readLength();

		std::vector<std::byte> m_data;
		size_t m_pos = 0;
		bool m_overrun = false;
	};
}
