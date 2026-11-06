#pragma once

#include <filesystem>

struct SDL_Cursor;

namespace oc4::render
{
	// A Windows cursor file (.cur, or .ani with its frames and rates) as an SDL cursor; null when it cannot be read.
	SDL_Cursor* loadCursorFile(const std::filesystem::path& file);
}
