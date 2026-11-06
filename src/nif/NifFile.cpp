#include "nif/NifFile.h"

#include "core/FileUtil.h"
#include "nif/NifBlocks.h"
#include "nif/NifReader.h"

#include <algorithm>
#include <format>

namespace oc4::nif
{
	namespace
	{
		Header readHeader(NifReader& r)
		{
			Header h;
			h.versionString = r.readLine();
			if (!h.versionString.starts_with("Gamebryo File Format, Version ") &&
				!h.versionString.starts_with("NetImmerse File Format, Version "))
				r.fail("not a NIF file");

			h.version = r.u32();
			r.version = h.version;
			// Before 5.0.0.1 (4.x, as some mods' older art): the block count, then the blocks, each
			// with its type's name in front.
			if (h.version < makeVersion(5, 0, 0, 1))
			{
				if (h.version < makeVersion(4, 0, 0, 0))
					r.fail(std::format("NIF version 0x{:08X} too old", h.version));
				h.numBlocks = r.u32();
				h.inlineTypes = true;
				return h;
			}
			if (h.version >= makeVersion(20, 1, 0, 1))
				r.fail(std::format("NIF version 0x{:08X} not supported yet", h.version));

			if (r.since(makeVersion(20, 0, 0, 3)))
				h.endian = r.u8();
			if (h.endian != 1)
				r.fail("big-endian NIF not supported");
			if (r.since(makeVersion(10, 0, 1, 8)))
				h.userVersion = r.u32();
			r.userVersion = h.userVersion;
			h.numBlocks = r.u32();

			// Bethesda extended header; Civ4 files have user version 0.
			if (h.userVersion >= 3)
				r.fail(std::format("user version {} (Bethesda header) not supported", h.userVersion));

			const uint16_t numTypes = r.u16();
			h.blockTypes.reserve(numTypes);
			for (uint16_t i = 0; i < numTypes; ++i)
				h.blockTypes.push_back(r.readSizedString(256));
			h.blockTypeIndex = r.readVector<uint16_t>(h.numBlocks);
			for (const uint16_t idx : h.blockTypeIndex)
				if (idx >= numTypes)
					r.fail("block type index out of range");

			if (r.since(makeVersion(5, 0, 0, 6)))
			{
				const uint32_t numGroups = r.u32();
				h.groups = r.readVector<uint32_t>(numGroups);
			}
			return h;
		}
	}

	Header parseHeaderOnly(std::span<const std::byte> data)
	{
		NifReader r(data);
		return readHeader(r);
	}

	File parseFile(std::span<const std::byte> data)
	{
		File file;
		NifReader r(data);
		file.header = readHeader(r);

		file.blocks.reserve(file.header.numBlocks);
		if (file.header.inlineTypes)
			file.header.blockTypes.reserve(file.header.numBlocks); // (the blocks point at their names)
		size_t blockIndex = 0;
		try
		{
			for (; blockIndex < file.header.numBlocks; ++blockIndex)
			{
				if (file.header.inlineTypes)
				{
					std::string name = r.readSizedString(256);
					auto& types = file.header.blockTypes;
					const auto at = std::find(types.begin(), types.end(), name);
					file.header.blockTypeIndex.push_back(uint16_t(at - types.begin()));
					if (at == types.end())
						types.push_back(std::move(name));
				}
				const std::string& type = file.header.blockTypeName(blockIndex);
				// 10.0.1.0 .. 10.1.x write a zero uint32 before every block.
				if (r.since(makeVersion(10, 0, 1, 0)) && r.version < makeVersion(10, 2, 0, 0) && r.u32() != 0)
					r.fail("expected zero block separator");
				std::unique_ptr<NiObject> block = parseBlock(r, type);
				block->typeName = &type;
				file.blocks.push_back(std::move(block));
			}

			file.roots = r.refList();
			if (!r.atEnd())
				r.fail(std::format("{} trailing bytes after footer", r.remaining()));
		}
		catch (const std::exception& e)
		{
			file.error = e.what();
			file.failedBlock = blockIndex;
		}
		return file;
	}

	File loadFile(const std::filesystem::path& path)
	{
		const auto data = readWholeFile(path);
		return parseFile(data);
	}
}
