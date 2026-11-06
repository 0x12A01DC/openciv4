#pragma once

// What the game core sees of the process, through the Windows functions it imports
// (the host patches the game core's import table): it runs like inside the original
// engine, which nothing in a headless host would otherwise make true.

#include <filesystem>
#include <optional>

namespace oc4::host
{
	// Loads the game core (OC4_GAME_CORE_DLL, delay-loaded): the mod's own from its Assets folder,
	// else the one beside the executable. Then its message boxes go to stderr, each answered with its
	// default button (Ignore for Abort/Retry/Ignore); its SetUnhandledExceptionFilter does nothing, so
	// the engine's crash handler stays; and GetACP answers Windows-1252, as BUG-based DLLs (AND2, CoM)
	// turn their UTF-8 texts into that code page and the window draws them as Latin-1.
	std::filesystem::path loadGameCore(const std::optional<std::filesystem::path>& modAssets);

	// GetModuleFileName answers with the original install's layout: the executable in the
	// Beyond the Sword folder, the game core in the Assets folder of its mod (or of the
	// game). BUG-based DLLs find the mod's files from their own location (AND2 loads its
	// XML modules from there).
	void setGameCoreModulePaths(const std::filesystem::path& exe, const std::filesystem::path& gameCore);
}
