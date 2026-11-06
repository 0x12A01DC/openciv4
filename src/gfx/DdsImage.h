#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace oc4::gfx
{
	// Formats that occur in Civ4 assets. Uncompressed sources are expanded to BGRA8.
	enum class PixelFormat
	{
		BC1, // DXT1
		BC2, // DXT3
		BC3, // DXT5
		BGRA8,
	};

	struct ImageMip
	{
		uint32_t width = 0, height = 0;
		uint32_t rowPitch = 0; // bytes per row (of blocks for BC formats)
		size_t offset = 0, size = 0;
	};

	struct Image
	{
		PixelFormat format = PixelFormat::BGRA8;
		uint32_t width = 0, height = 0;
		std::vector<ImageMip> mips;
		std::vector<std::byte> data;
		bool hasAlpha = false;

		std::span<const std::byte> mipData(size_t i) const { return { data.data() + mips[i].offset, mips[i].size }; }
	};

	// Throws ParseError on unsupported or malformed files.
	Image loadDds(std::span<const std::byte> file);
	// A TGA picture (true colour, grey or colour mapped; plain or RLE), as BGRA8 with its mips.
	Image loadTga(std::span<const std::byte> file);
	// A model's texture: DDS, or the TGA files mods use too.
	Image loadTexture(std::span<const std::byte> file);

	// One mip level as BGRA8 pixels (BC formats decoded), width * height * 4 bytes.
	std::vector<uint8_t> decodeBgra(const Image& image, size_t mip = 0);
}
