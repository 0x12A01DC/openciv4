#include "gfx/DdsImage.h"

#include "core/BinaryReader.h"

#include <algorithm>
#include <cstring>
#include <bit>
#include <format>

namespace oc4::gfx
{
	namespace
	{
		constexpr uint32_t fourCC(char a, char b, char c, char d)
		{
			return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8) | (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(d)) << 24);
		}

		constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;
		constexpr uint32_t DDPF_FOURCC = 0x4;
		constexpr uint32_t DDPF_RGB = 0x40;
		constexpr uint32_t DDSD_MIPMAPCOUNT = 0x20000;

		// Extracts a channel described by a bit mask and rescales it to 8 bits.
		uint8_t channel(uint32_t pixel, uint32_t mask)
		{
			if (mask == 0)
				return 255;
			const int shift = std::countr_zero(mask);
			const int bits = std::popcount(mask);
			const uint32_t v = (pixel & mask) >> shift;
			const uint32_t maxv = (1u << bits) - 1;
			return static_cast<uint8_t>((v * 255 + maxv / 2) / maxv);
		}
	}

	Image loadDds(std::span<const std::byte> file)
	{
		BinaryReader r(file);
		if (r.u32() != fourCC('D', 'D', 'S', ' '))
			r.fail("not a DDS file");
		if (r.u32() != 124)
			r.fail("bad DDS header size");
		const uint32_t flags = r.u32();
		const uint32_t height = r.u32();
		const uint32_t width = r.u32();
		(void)r.u32(); // pitch or linear size
		(void)r.u32(); // depth
		uint32_t mipCount = r.u32();
		r.skip(11 * 4);
		if (r.u32() != 32)
			r.fail("bad DDS pixel format size");
		const uint32_t pfFlags = r.u32();
		const uint32_t pfFourCC = r.u32();
		const uint32_t bitCount = r.u32();
		const uint32_t masks[4] = { r.u32(), r.u32(), r.u32(), r.u32() }; // r g b a
		const uint32_t caps1 = r.u32();
		const uint32_t caps2 = r.u32();
		r.skip(3 * 4);
		(void)caps1;
		if (caps2 & 0x200)
			r.fail("cube maps not supported");
		if (width == 0 || height == 0 || width > 16384 || height > 16384)
			r.fail(std::format("bad DDS size {}x{}", width, height));
		if (!(flags & DDSD_MIPMAPCOUNT) || mipCount == 0)
			mipCount = 1;
		mipCount = std::min<uint32_t>(mipCount, std::bit_width(std::max(width, height)));

		Image img;
		img.width = width;
		img.height = height;

		if (pfFlags & DDPF_FOURCC)
		{
			size_t blockBytes = 16;
			switch (pfFourCC)
			{
			case fourCC('D', 'X', 'T', '1'): img.format = PixelFormat::BC1; blockBytes = 8; break;
			case fourCC('D', 'X', 'T', '2'):
			case fourCC('D', 'X', 'T', '3'): img.format = PixelFormat::BC2; img.hasAlpha = true; break;
			case fourCC('D', 'X', 'T', '4'):
			case fourCC('D', 'X', 'T', '5'): img.format = PixelFormat::BC3; img.hasAlpha = true; break;
			default: r.fail("unsupported DDS FourCC");
			}
			size_t offset = 0;
			uint32_t w = width, h = height;
			for (uint32_t i = 0; i < mipCount; ++i)
			{
				const uint32_t bw = std::max(1u, (w + 3) / 4), bh = std::max(1u, (h + 3) / 4);
				const size_t size = size_t(bw) * bh * blockBytes;
				if (offset + size > r.remaining())
				{
					if (i == 0)
						r.fail("truncated DDS data");
					break; // keep the mips we have
				}
				img.mips.push_back({ w, h, static_cast<uint32_t>(bw * blockBytes), offset, size });
				offset += size;
				w = std::max(1u, w / 2);
				h = std::max(1u, h / 2);
			}
			img.data.resize(offset);
			r.readInto(img.data.data(), offset);
			// BC1 has 1-bit alpha; Civ4 relies on it for cut-outs.
			if (img.format == PixelFormat::BC1)
				img.hasAlpha = true;
			return img;
		}

		if (!(pfFlags & DDPF_RGB) || (bitCount != 16 && bitCount != 24 && bitCount != 32))
			r.fail("unsupported DDS pixel format");

		img.format = PixelFormat::BGRA8;
		img.hasAlpha = (pfFlags & DDPF_ALPHAPIXELS) && masks[3] != 0;
		const uint32_t bytesPerPixel = bitCount / 8;
		uint32_t w = width, h = height;
		for (uint32_t i = 0; i < mipCount; ++i)
		{
			const size_t srcSize = size_t(w) * h * bytesPerPixel;
			if (srcSize > r.remaining())
			{
				if (i == 0)
					r.fail("truncated DDS data");
				break;
			}
			const size_t offset = img.data.size();
			img.data.resize(offset + size_t(w) * h * 4);
			auto* dst = reinterpret_cast<uint8_t*>(img.data.data() + offset);
			for (size_t p = 0; p < size_t(w) * h; ++p)
			{
				uint32_t pixel = 0;
				for (uint32_t b = 0; b < bytesPerPixel; ++b)
					pixel |= uint32_t(r.u8()) << (8 * b);
				dst[p * 4 + 0] = channel(pixel, masks[2]);
				dst[p * 4 + 1] = channel(pixel, masks[1]);
				dst[p * 4 + 2] = channel(pixel, masks[0]);
				dst[p * 4 + 3] = img.hasAlpha ? channel(pixel, masks[3]) : 255;
			}
			img.mips.push_back({ w, h, w * 4, offset, size_t(w) * h * 4 });
			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);
		}
		return img;
	}

	namespace
	{
		// RGB565 colour endpoints and the two interpolated ones, BGRA8 each.
		void bc1Palette(uint16_t c0, uint16_t c1, bool threeColour, uint8_t out[4][4])
		{
			const auto expand = [](uint16_t c, uint8_t* o) {
				o[2] = uint8_t(((c >> 11) & 31) * 255 / 31);
				o[1] = uint8_t(((c >> 5) & 63) * 255 / 63);
				o[0] = uint8_t((c & 31) * 255 / 31);
				o[3] = 255;
			};
			expand(c0, out[0]);
			expand(c1, out[1]);
			for (int k = 0; k < 3; ++k)
				if (threeColour)
				{
					out[2][k] = uint8_t((out[0][k] + out[1][k]) / 2);
					out[3][k] = 0;
				}
				else
				{
					out[2][k] = uint8_t((2 * out[0][k] + out[1][k]) / 3);
					out[3][k] = uint8_t((out[0][k] + 2 * out[1][k]) / 3);
				}
			out[2][3] = 255;
			out[3][3] = threeColour ? 0 : 255;
		}
	}

	std::vector<uint8_t> decodeBgra(const Image& image, size_t mip)
	{
		const ImageMip& m = image.mips.at(mip);
		const auto src = image.mipData(mip);
		std::vector<uint8_t> out(size_t(m.width) * m.height * 4);
		if (image.format == PixelFormat::BGRA8)
		{
			std::memcpy(out.data(), src.data(), std::min(out.size(), src.size()));
			return out;
		}
		const size_t blockBytes = image.format == PixelFormat::BC1 ? 8 : 16;
		const uint32_t bw = (m.width + 3) / 4, bh = (m.height + 3) / 4;
		const auto* bytes = reinterpret_cast<const uint8_t*>(src.data());
		for (uint32_t by = 0; by < bh; ++by)
			for (uint32_t bx = 0; bx < bw; ++bx)
			{
				const size_t at = (size_t(by) * bw + bx) * blockBytes;
				if (at + blockBytes > src.size())
					return out;
				const uint8_t* block = bytes + at;
				const uint8_t* colour = image.format == PixelFormat::BC1 ? block : block + 8;
				const uint16_t c0 = uint16_t(colour[0] | colour[1] << 8), c1 = uint16_t(colour[2] | colour[3] << 8);
				uint8_t palette[4][4];
				bc1Palette(c0, c1, image.format == PixelFormat::BC1 && c0 <= c1, palette);
				const uint32_t indices = uint32_t(colour[4]) | uint32_t(colour[5]) << 8 | uint32_t(colour[6]) << 16 | uint32_t(colour[7]) << 24;

				uint8_t alpha[16];
				if (image.format == PixelFormat::BC2)
					for (int i = 0; i < 16; ++i)
						alpha[i] = uint8_t(((block[i / 2] >> (4 * (i & 1))) & 15) * 17);
				else if (image.format == PixelFormat::BC3)
				{
					uint8_t a[8] = { block[0], block[1] };
					for (int i = 2; i < 8; ++i)
						a[i] = a[0] > a[1] ? uint8_t(((8 - i) * a[0] + (i - 1) * a[1]) / 7)
							: i < 6 ? uint8_t(((6 - i) * a[0] + (i - 1) * a[1]) / 5) : i == 6 ? 0 : 255;
					uint64_t bits = 0;
					for (int i = 0; i < 6; ++i)
						bits |= uint64_t(block[2 + i]) << (8 * i);
					for (int i = 0; i < 16; ++i)
						alpha[i] = a[(bits >> (3 * i)) & 7];
				}
				for (int py = 0; py < 4; ++py)
					for (int px = 0; px < 4; ++px)
					{
						const uint32_t x = bx * 4 + px, y = by * 4 + py;
						if (x >= m.width || y >= m.height)
							continue;
						const int i = py * 4 + px;
						uint8_t* d = out.data() + (size_t(y) * m.width + x) * 4;
						std::memcpy(d, palette[(indices >> (2 * i)) & 3], 4);
						if (image.format != PixelFormat::BC1)
							d[3] = alpha[i];
					}
			}
		return out;
	}
}
