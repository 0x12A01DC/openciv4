#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace oc4::render
{
	// Writes BGRA8 pixels as PNG via WIC. Throws on failure.
	void writePng(const std::filesystem::path& path, const std::vector<uint8_t>& bgra, uint32_t width, uint32_t height);
}
