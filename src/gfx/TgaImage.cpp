#include "gfx/DdsImage.h"

#include "core/BinaryReader.h"

#include <algorithm>
#include <cstring>

namespace oc4::gfx
{
	namespace
	{
		// The next level of a BGRA8 mip chain: each pixel the mean of the (up to) four under it.
		void halve(const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst, uint32_t nw, uint32_t nh)
		{
			for (uint32_t y = 0; y < nh; ++y)
				for (uint32_t x = 0; x < nw; ++x)
				{
					const uint32_t x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
					const uint32_t y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
					for (int c = 0; c < 4; ++c)
					{
						const uint32_t sum = src[(size_t(y0) * w + x0) * 4 + c] + src[(size_t(y0) * w + x1) * 4 + c] + src[(size_t(y1) * w + x0) * 4 + c] +
							src[(size_t(y1) * w + x1) * 4 + c];
						dst[(size_t(y) * nw + x) * 4 + c] = uint8_t((sum + 2) / 4);
					}
				}
		}
	}

	Image loadTga(std::span<const std::byte> file)
	{
		const auto fail = [](const char* what, size_t at) { throw ParseError(std::string("TGA: ") + what, at); };
		if (file.size() < 18)
			fail("too short", 0);
		const auto u8 = [&](size_t i) {
			if (i >= file.size())
				fail("truncated", i);
			return uint8_t(file[i]);
		};
		const uint8_t idLength = u8(0), mapType = u8(1), type = u8(2);
		const uint32_t mapFirst = u8(3) | (u8(4) << 8), mapLength = u8(5) | (u8(6) << 8), mapBits = u8(7);
		const uint32_t width = u8(12) | (u8(13) << 8), height = u8(14) | (u8(15) << 8), bits = u8(16);
		const uint8_t descriptor = u8(17);
		const bool rle = type >= 9, mapped = (type & 7) == 1, grey = (type & 7) == 3;
		if ((type & 7) < 1 || (type & 7) > 3 || width == 0 || height == 0)
			fail("unsupported image type", 2);
		if (mapped ? (bits != 8 || !mapType) : grey ? bits != 8 && bits != 16 : bits != 15 && bits != 16 && bits != 24 && bits != 32)
			fail("unsupported pixel size", 16);

		// A pixel's bytes as BGRA (straight alpha).
		const auto colour = [&](size_t at, uint32_t size, uint8_t* out) {
			if (size == 1)
				out[0] = out[1] = out[2] = u8(at), out[3] = 255;
			else if (size == 2 && grey)
				out[0] = out[1] = out[2] = u8(at), out[3] = u8(at + 1);
			else if (size == 2)
			{
				const uint32_t v = u8(at) | (u8(at + 1) << 8);
				const auto five = [](uint32_t c) { return uint8_t((c << 3) | (c >> 2)); };
				out[0] = five(v & 31), out[1] = five((v >> 5) & 31), out[2] = five((v >> 10) & 31);
				out[3] = bits == 16 && (descriptor & 15) && !(v & 0x8000) ? 0 : 255;
			}
			else
				out[0] = u8(at), out[1] = u8(at + 1), out[2] = u8(at + 2), out[3] = size == 4 ? u8(at + 3) : 255;
		};

		size_t p = 18 + idLength;
		// (a colour map: its entries as BGRA)
		const uint32_t entryBytes = (mapBits + 7) / 8;
		std::vector<uint8_t> palette;
		if (mapType)
		{
			if (mapped)
			{
				palette.resize(size_t(mapFirst + mapLength) * 4);
				for (uint32_t i = 0; i < mapLength; ++i)
					colour(p + size_t(i) * entryBytes, entryBytes, &palette[size_t(mapFirst + i) * 4]);
			}
			p += size_t(mapLength) * entryBytes;
		}
		const uint32_t bpp = (bits + 7) / 8;
		const size_t pixels = size_t(width) * height;
		std::vector<uint8_t> raw(pixels * 4);
		const auto pixel = [&](size_t i, size_t at) {
			if (mapped)
			{
				const size_t index = u8(at);
				if ((index + 1) * 4 <= palette.size())
					std::memcpy(&raw[i * 4], &palette[index * 4], 4);
			}
			else
				colour(at, bpp, &raw[i * 4]);
		};
		if (!rle)
			for (size_t i = 0; i < pixels; ++i)
				pixel(i, p + i * bpp);
		else
			for (size_t i = 0; i < pixels;)
			{
				const uint8_t head = u8(p++);
				const size_t count = (head & 0x7F) + 1;
				for (size_t k = 0; k < count && i < pixels; ++k)
				{
					pixel(i++, p);
					if (!(head & 0x80))
						p += bpp;
				}
				if (head & 0x80)
					p += bpp;
			}

		// Top-down (the origin is at the bottom unless the descriptor says top), and its mips.
		Image img;
		img.format = PixelFormat::BGRA8;
		img.width = width;
		img.height = height;
		const bool topDown = (descriptor & 0x20) != 0, rightToLeft = (descriptor & 0x10) != 0;
		std::vector<uint8_t> level(pixels * 4);
		for (uint32_t y = 0; y < height; ++y)
			for (uint32_t x = 0; x < width; ++x)
			{
				const uint32_t sy = topDown ? y : height - 1 - y, sx = rightToLeft ? width - 1 - x : x;
				std::memcpy(&level[(size_t(y) * width + x) * 4], &raw[(size_t(sy) * width + sx) * 4], 4);
			}
		for (size_t i = 0; i < pixels && !img.hasAlpha; ++i)
			img.hasAlpha = level[i * 4 + 3] != 255;
		uint32_t w = width, h = height;
		for (;;)
		{
			const size_t offset = img.data.size(), size = size_t(w) * h * 4;
			img.mips.push_back({ w, h, w * 4, offset, size });
			img.data.resize(offset + size);
			std::memcpy(img.data.data() + offset, level.data(), size);
			if (w == 1 && h == 1)
				break;
			const uint32_t nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
			std::vector<uint8_t> next(size_t(nw) * nh * 4);
			halve(level.data(), w, h, next.data(), nw, nh);
			level = std::move(next);
			w = nw;
			h = nh;
		}
		return img;
	}

	Image loadTexture(std::span<const std::byte> file)
	{
		if (file.size() >= 4 && std::memcmp(file.data(), "DDS ", 4) == 0)
			return loadDds(file);
		return loadTga(file);
	}
}
