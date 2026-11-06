#pragma once

#include "nif/NifTypes.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace oc4::nif
{
	struct Header
	{
		std::string versionString;
		uint32_t version = 0;
		uint8_t endian = 1;
		uint32_t userVersion = 0;
		uint32_t numBlocks = 0;
		std::vector<std::string> blockTypes;
		std::vector<uint16_t> blockTypeIndex;
		std::vector<uint32_t> groups;
		bool inlineTypes = false; // (before 5.0.0.1: each block starts with its type's name, no table)

		const std::string& blockTypeName(size_t block) const { return blockTypes.at(blockTypeIndex.at(block)); }
	};

	struct NiObject
	{
		virtual ~NiObject() = default;
		const std::string* typeName = nullptr; // points into Header::blockTypes
	};

	struct File
	{
		Header header;
		std::vector<std::unique_ptr<NiObject>> blocks;
		std::vector<Ref> roots;

		// Set when parsing stopped early; blocks parsed before the failure are kept.
		std::optional<std::string> error;
		std::optional<size_t> failedBlock;

		bool complete() const { return !error.has_value(); }

		template <class T>
		const T* get(Ref ref) const
		{
			if (ref < 0 || static_cast<size_t>(ref) >= blocks.size())
				return nullptr;
			return dynamic_cast<const T*>(blocks[static_cast<size_t>(ref)].get());
		}

		const std::string& typeOf(Ref ref) const { return header.blockTypeName(static_cast<size_t>(ref)); }
	};

	Header parseHeaderOnly(std::span<const std::byte> data);

	// Parses header, all blocks and the footer. Never throws on malformed block
	// data: the error is recorded in File::error and parsing stops (NIF 20.0.0.4
	// has no block sizes, so an unknown block cannot be skipped).
	File parseFile(std::span<const std::byte> data);
	File loadFile(const std::filesystem::path& path);
}
