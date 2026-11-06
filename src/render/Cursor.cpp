#include "render/Cursor.h"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_surface.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace oc4::render
{
	namespace
	{
		uint16_t u16(std::span<const uint8_t> d, size_t at) { return at + 2 <= d.size() ? uint16_t(d[at] | d[at + 1] << 8) : 0; }
		uint32_t u32(std::span<const uint8_t> d, size_t at) { return at + 4 <= d.size() ? uint32_t(u16(d, at) | u16(d, at + 2) << 16) : 0; }

		struct Image
		{
			int width = 0, height = 0, hotX = 0, hotY = 0;
			std::vector<uint8_t> bgra;
		};

		// The first image of an icon or cursor resource (ICONDIR): a DIB with its AND mask (PNG ones are not read).
		std::optional<Image> readIcon(std::span<const uint8_t> d)
		{
			if (u16(d, 0) != 0 || u16(d, 4) == 0 || d.size() < 22)
				return std::nullopt;
			Image img;
			img.hotX = u16(d, 10);
			img.hotY = u16(d, 12);
			const size_t size = u32(d, 14), at = u32(d, 18);
			if (at + 40 > d.size() || at + size > d.size())
				return std::nullopt;
			const auto dib = d.subspan(at, size);
			const int w = int(u32(dib, 4)), h = int(int32_t(u32(dib, 8))) / 2, bpp = u16(dib, 14);
			if (u32(dib, 0) < 40 || w <= 0 || h <= 0 || w > 256 || h > 256 || u32(dib, 16) != 0)
				return std::nullopt;
			const size_t colors = bpp <= 8 ? (u32(dib, 32) ? u32(dib, 32) : size_t(1) << bpp) : 0;
			const size_t palette = u32(dib, 0), pixels = palette + colors * 4;
			const size_t stride = (size_t(w) * bpp + 31) / 32 * 4, maskStride = (size_t(w) + 31) / 32 * 4;
			const size_t mask = pixels + stride * size_t(h);
			if (mask + maskStride * size_t(h) > dib.size())
				return std::nullopt;
			img.width = w, img.height = h;
			img.bgra.resize(size_t(w) * size_t(h) * 4);
			bool alpha = false;
			for (int y = 0; y < h; ++y)
				for (int x = 0; x < w; ++x)
				{
					const size_t row = pixels + stride * size_t(h - 1 - y);
					uint8_t* out = &img.bgra[(size_t(y) * size_t(w) + size_t(x)) * 4];
					if (bpp == 32 || bpp == 24)
					{
						const size_t p = row + size_t(x) * size_t(bpp / 8);
						out[0] = dib[p], out[1] = dib[p + 1], out[2] = dib[p + 2], out[3] = bpp == 32 ? dib[p + 3] : 0;
						alpha |= out[3] != 0;
					}
					else if (bpp == 8 || bpp == 4 || bpp == 1)
					{
						const size_t bit = size_t(x) * size_t(bpp);
						const unsigned index = (dib[row + bit / 8] >> (8 - bpp - bit % 8)) & ((1u << bpp) - 1);
						if (index >= colors)
							continue;
						std::memcpy(out, &dib[palette + index * 4], 3);
					}
					else
						return std::nullopt;
				}
			if (!alpha)
				for (int y = 0; y < h; ++y)
					for (int x = 0; x < w; ++x)
					{
						const bool transparent = (dib[mask + maskStride * size_t(h - 1 - y) + size_t(x) / 8] >> (7 - x % 8)) & 1;
						uint8_t* out = &img.bgra[(size_t(y) * size_t(w) + size_t(x)) * 4];
						// (a pixel the mask keeps and the colour inverts: drawn black, as nothing here inverts)
						out[3] = transparent && (out[0] | out[1] | out[2]) == 0 ? 0 : 255;
						if (transparent)
							out[0] = out[1] = out[2] = 0;
					}
			return img;
		}

		struct SurfaceDeleter
		{
			void operator()(SDL_Surface* s) const { SDL_DestroySurface(s); }
		};
		using Surface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

		Surface surfaceOf(const Image& img)
		{
			Surface s(SDL_CreateSurface(img.width, img.height, SDL_PIXELFORMAT_BGRA32));
			if (s)
				for (int y = 0; y < img.height; ++y)
					std::memcpy(static_cast<uint8_t*>(s->pixels) + size_t(y) * size_t(s->pitch), &img.bgra[size_t(y) * size_t(img.width) * 4],
						size_t(img.width) * 4);
			return s;
		}
	}

	SDL_Cursor* loadCursorFile(const std::filesystem::path& file)
	{
		std::ifstream in(file, std::ios::binary);
		const std::vector<uint8_t> data{ std::istreambuf_iterator<char>(in), {} };
		const std::span<const uint8_t> d(data);
		if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(&data[8], "ACON", 4) != 0)
		{
			const auto img = readIcon(d);
			const Surface s = img ? surfaceOf(*img) : nullptr;
			return s ? SDL_CreateColorCursor(s.get(), img->hotX, img->hotY) : nullptr;
		}
		uint32_t steps = 0, jiffies = 10;
		std::vector<uint32_t> rates, sequence;
		std::vector<Image> frames;
		for (size_t at = 12; at + 8 <= d.size();)
		{
			const auto id = d.subspan(at, 4);
			const size_t size = u32(d, at + 4), body = at + 8;
			if (body + size > d.size())
				break;
			const auto list = [&](std::vector<uint32_t>& v) {
				for (size_t i = 0; i + 4 <= size; i += 4)
					v.push_back(u32(d, body + i));
			};
			if (std::memcmp(id.data(), "anih", 4) == 0)
			{
				steps = u32(d, body + 8);
				jiffies = u32(d, body + 28);
			}
			else if (std::memcmp(id.data(), "rate", 4) == 0)
				list(rates);
			else if (std::memcmp(id.data(), "seq ", 4) == 0)
				list(sequence);
			else if (std::memcmp(id.data(), "LIST", 4) == 0 && size >= 4 && std::memcmp(&d[body], "fram", 4) == 0)
				for (size_t i = body + 4; i + 8 <= body + size;)
				{
					const size_t n = u32(d, i + 4);
					if (std::memcmp(&d[i], "icon", 4) == 0 && i + 8 + n <= body + size)
						if (auto img = readIcon(d.subspan(i + 8, n)))
							frames.push_back(std::move(*img));
					i += 8 + n + (n & 1);
				}
			at = body + size + (size & 1);
		}
		if (frames.empty())
			return nullptr;
		std::vector<Surface> surfaces;
		for (const Image& img : frames)
		{
			if (img.width != frames[0].width || img.height != frames[0].height)
				return nullptr;
			surfaces.push_back(surfaceOf(img));
			if (!surfaces.back())
				return nullptr;
		}
		if (steps == 0)
			steps = uint32_t(frames.size());
		std::vector<SDL_CursorFrameInfo> info;
		for (uint32_t i = 0; i < steps; ++i)
		{
			const uint32_t frame = i < sequence.size() ? sequence[i] : i;
			if (frame >= surfaces.size())
				return nullptr;
			info.push_back({ surfaces[frame].get(), (i < rates.size() ? rates[i] : jiffies) * 1000 / 60 });
		}
		if (info.size() == 1)
			return SDL_CreateColorCursor(info[0].surface, frames[0].hotX, frames[0].hotY);
		return SDL_CreateAnimatedCursor(info.data(), int(info.size()), frames[0].hotX, frames[0].hotY);
	}
}
