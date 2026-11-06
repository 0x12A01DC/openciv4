#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace oc4
{
	class ParseError : public std::runtime_error
	{
	public:
		ParseError(std::string message, size_t offset)
			: std::runtime_error(std::move(message)), m_offset(offset)
		{
		}

		size_t offset() const { return m_offset; }

	private:
		size_t m_offset;
	};

	// Little-endian reader over an in-memory buffer. Every read is bounds-checked,
	// and array reads refuse lengths that cannot fit in the remaining data, so a
	// misparse fails fast instead of allocating gigabytes.
	class BinaryReader
	{
	public:
		explicit BinaryReader(std::span<const std::byte> data) : m_data(data) {}

		size_t pos() const { return m_pos; }
		size_t size() const { return m_data.size(); }
		size_t remaining() const { return m_data.size() - m_pos; }
		bool atEnd() const { return m_pos == m_data.size(); }

		void seek(size_t pos)
		{
			if (pos > m_data.size())
				fail("seek past end");
			m_pos = pos;
		}

		void skip(size_t n)
		{
			require(n);
			m_pos += n;
		}

		template <class T>
		T read()
		{
			static_assert(std::is_trivially_copyable_v<T>);
			require(sizeof(T));
			T value;
			std::memcpy(&value, m_data.data() + m_pos, sizeof(T));
			m_pos += sizeof(T);
			return value;
		}

		uint8_t u8() { return read<uint8_t>(); }
		uint16_t u16() { return read<uint16_t>(); }
		uint32_t u32() { return read<uint32_t>(); }
		int16_t i16() { return read<int16_t>(); }
		int32_t i32() { return read<int32_t>(); }
		float f32() { return read<float>(); }

		template <class T>
		void readInto(T* out, size_t count)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			requireArray(count, sizeof(T));
			std::memcpy(out, m_data.data() + m_pos, count * sizeof(T));
			m_pos += count * sizeof(T);
		}

		template <class T>
		std::vector<T> readVector(size_t count)
		{
			requireArray(count, sizeof(T));
			std::vector<T> out(count);
			readInto(out.data(), count);
			return out;
		}

		// Reads bytes up to and including '\n'; the newline is not returned.
		std::string readLine(size_t maxLen = 256);

		// uint32 length + bytes.
		std::string readSizedString(size_t maxLen = 1u << 20);

		[[noreturn]] void fail(std::string_view what) const;

		void requireArray(size_t count, size_t elemSize) const
		{
			if (elemSize != 0 && count > remaining() / elemSize)
				fail("array length exceeds remaining data");
		}

	private:
		void require(size_t n) const
		{
			if (n > remaining())
				fail("unexpected end of data");
		}

		std::span<const std::byte> m_data;
		size_t m_pos = 0;
	};
}
