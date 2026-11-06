#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace oc4
{
	std::vector<std::byte> readWholeFile(const std::filesystem::path& path);

	// Lower-cases ASCII letters; Civ4 asset paths are case-insensitive.
	std::string toLowerAscii(std::string s);

	// A path as UTF-8, for names found on disk: path::string() converts to the system's
	// code page and throws on what it cannot hold (a mod's "b5 másolata.dds" on a Cyrillic
	// system). The game never refers to such names, so they only need not stop a scan.
	std::string utf8Path(const std::filesystem::path& path);
	std::string utf8GenericPath(const std::filesystem::path& path);
}
