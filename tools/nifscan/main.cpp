// nifscan: parses .nif/.kf files and reports block type usage, parse coverage
// and texture resolution. A file counts as parsed only if every block and the
// footer were read and the reader ended exactly at EOF.
//
//   nifscan --civ4 <Civ4 root>        every model in all layers, including FPK archives
//   nifscan <dir> [<dir>...]          loose files under the given folders
//   options: --errors N               error samples per category

#include "core/AssetResolver.h"
#include "core/FileUtil.h"
#include "nif/KfmFile.h"
#include "nif/NifBlocks.h"
#include "nif/NifFile.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace oc4;

namespace
{
	std::string versionToString(uint32_t v)
	{
		return std::format("{}.{}.{}.{}", v >> 24, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
	}

	struct TypeStats
	{
		size_t files = 0;
		size_t blocks = 0;
	};

	struct Input
	{
		std::string name;
		std::optional<AssetLocation> location; // for texture lookup and packed reads
		fs::path file;
	};
}

int wmain(int argc, wchar_t** argv)
{
	std::vector<fs::path> roots;
	size_t maxErrorSamples = 10;
	std::optional<AssetResolver> resolver;
	for (int i = 1; i < argc; ++i)
	{
		const std::wstring arg = argv[i];
		if (arg == L"--errors" && i + 1 < argc)
			maxErrorSamples = std::stoul(argv[++i]);
		else if (arg == L"--civ4" && i + 1 < argc)
			resolver = AssetResolver::forInstall(argv[++i]);
		else
			roots.emplace_back(arg);
	}
	if (roots.empty() && !resolver)
	{
		std::cerr << "usage: nifscan --civ4 <Civ4 root> | <dir> [<dir>...]  [--errors N]\n";
		return 2;
	}

	std::vector<Input> inputs;
	if (roots.empty())
	{
		for (const char* ext : { ".nif", ".kf" })
			for (auto& loc : resolver->list(ext))
				inputs.push_back({ loc.display(), loc, loc.file });
	}
	else
	{
		for (const auto& root : roots)
			for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
				if (entry.is_regular_file())
				{
					const std::string ext = toLowerAscii(entry.path().extension().string());
					if (ext == ".nif" || ext == ".kf")
						inputs.push_back({ entry.path().string(), resolver ? resolver->locateFile(entry.path()) : std::nullopt, entry.path() });
				}
		std::ranges::sort(inputs, {}, &Input::name);
	}

	std::map<std::string, size_t> versions;
	std::map<std::string, TypeStats> types;
	std::map<std::string, size_t> firstFailureType;
	std::map<std::string, std::vector<std::string>> errorSamples;
	size_t headerFailures = 0, complete = 0, packed = 0;

	std::map<std::string, size_t> textureExtensions;
	size_t texturesFound = 0, texturesEmbedded = 0, texturesUnresolved = 0;
	std::vector<std::string> unresolvedSamples;

	// file name -> folders that contain it, to discover texture search paths
	std::map<std::string, std::set<std::string>> textureFolders;
	if (resolver)
		for (const char* ext : { ".dds", ".tga", ".bmp" })
			for (const auto& loc : resolver->list(ext))
			{
				const size_t slash = loc.key.rfind('/');
				textureFolders[loc.key.substr(slash + 1)].insert(loc.key.substr(0, slash + 1));
			}
	std::map<std::string, size_t> unresolvedFoundIn;
	size_t unresolvedNowhere = 0;

	const auto t0 = std::chrono::steady_clock::now();
	for (const auto& input : inputs)
	{
		nif::File f;
		try
		{
			if (input.location && input.location->archive)
				++packed;
			const auto data = input.location ? resolver->read(*input.location) : readWholeFile(input.file);
			f = nif::parseFile(data);
		}
		catch (const std::exception& e)
		{
			++headerFailures;
			auto& samples = errorSamples["<header>"];
			if (samples.size() < maxErrorSamples)
				samples.push_back(std::format("{}: {}", input.name, e.what()));
			continue;
		}

		versions[std::format("{} uv={}", versionToString(f.header.version), f.header.userVersion)]++;

		std::set<std::string> seen;
		// (a file of the old kind names each block where it starts: as far as it was read)
		for (size_t i = 0; i < f.header.blockTypeIndex.size(); ++i)
		{
			const auto& t = f.header.blockTypeName(i);
			types[t].blocks++;
			if (seen.insert(t).second)
				types[t].files++;
		}

		if (f.complete())
			++complete;
		else
		{
			const std::string key = f.failedBlock && *f.failedBlock < f.header.blockTypeIndex.size()
				? f.header.blockTypeName(*f.failedBlock)
				: std::string("<footer>");
			firstFailureType[key]++;
			auto& samples = errorSamples[key];
			if (samples.size() < maxErrorSamples)
				samples.push_back(std::format("{} [block {}]: {}", input.name, f.failedBlock.value_or(0), *f.error));
		}

		if (!resolver || !input.location)
			continue;
		for (const auto& block : f.blocks)
			if (const auto* tex = dynamic_cast<const nif::NiSourceTexture*>(block.get()))
			{
				if (!tex->external)
				{
					++texturesEmbedded;
					continue;
				}
				textureExtensions[toLowerAscii(fs::path(tex->fileName).extension().string())]++;
				if (resolver->locateTexture(*input.location, tex->fileName))
					++texturesFound;
				else
				{
					++texturesUnresolved;
					if (unresolvedSamples.size() < maxErrorSamples)
						unresolvedSamples.push_back(std::format("{} -> '{}'", input.name, tex->fileName));
					std::string name = makeAssetKey(fs::path(tex->fileName).filename().string());
					auto it = textureFolders.find(name);
					if (it == textureFolders.end())
						it = textureFolders.find(fs::path(name).replace_extension(".dds").string());
					if (it == textureFolders.end())
						++unresolvedNowhere;
					else
						for (const auto& folder : it->second)
							unresolvedFoundIn[folder]++;
				}
			}
	}
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

	const size_t total = inputs.size();
	std::cout << std::format("files: {} ({} packed)  complete: {} ({:.1f}%)  read failures: {}  time: {:.2f}s\n\n",
		total, packed, complete, total ? 100.0 * complete / total : 0.0, headerFailures, seconds);

	std::cout << "versions:\n";
	for (const auto& [v, n] : versions)
		std::cout << std::format("  {:<24} {}\n", v, n);

	std::vector<std::pair<std::string, TypeStats>> sorted(types.begin(), types.end());
	std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.second.files > b.second.files; });
	std::cout << "\nblock types (files / blocks):\n";
	for (const auto& [t, s] : sorted)
		std::cout << std::format("  {:<40} {:>5} {:>7}\n", t, s.files, s.blocks);

	std::vector<std::pair<std::string, size_t>> failures(firstFailureType.begin(), firstFailureType.end());
	std::ranges::sort(failures, [](const auto& a, const auto& b) { return a.second > b.second; });
	std::cout << "\nparse stopped at (files):\n";
	for (const auto& [t, n] : failures)
		std::cout << std::format("  {:<40} {}\n", t, n);
	std::cout << "\nerror samples:\n";
	for (const auto& [t, n] : failures)
	{
		std::cout << "  [" << t << "]\n";
		for (const auto& s : errorSamples[t])
			std::cout << "    " << s << '\n';
	}
	for (const auto& s : errorSamples["<header>"])
		std::cout << "  [read] " << s << '\n';

	if (resolver)
	{
		std::cout << std::format("\ntextures: found {}, embedded {}, unresolved {}\n", texturesFound, texturesEmbedded, texturesUnresolved);
		for (const auto& [ext, n] : textureExtensions)
			std::cout << std::format("  {:<8} {}\n", ext.empty() ? "<none>" : ext, n);

		// KFM animation sets: parse, and check that every referenced .kf exists and
		// has the referenced sequence.
		size_t kfmOk = 0, kfmFailed = 0, animations = 0, missingKf = 0, badIndex = 0;
		std::map<std::string, size_t> kfmVersions;
		std::map<uint32_t, size_t> transitionTypes;
		std::vector<std::string> kfmErrors;
		std::map<std::string, size_t> sequenceCounts; // kf key -> number of NiControllerSequence roots
		for (const auto& loc : resolver->list(".kfm"))
		{
			try
			{
				const nif::KfmFile kfm = nif::parseKfm(resolver->read(loc));
				++kfmOk;
				kfmVersions[kfm.version]++;
				const std::string dir = loc.key.substr(0, loc.key.rfind('/') + 1);
				for (const auto& a : kfm.animations)
				{
					++animations;
					for (const auto& t : a.transitions)
						transitionTypes[uint32_t(t.type)]++;
					const std::string kfKey = makeAssetKey(dir + a.kfFile);
					auto it = sequenceCounts.find(kfKey);
					if (it == sequenceCounts.end())
					{
						size_t count = SIZE_MAX;
						if (const auto kf = resolver->locate(kfKey))
						{
							const nif::File f = nif::parseFile(resolver->read(*kf));
							count = 0;
							for (const nif::Ref root : f.roots)
								count += f.get<nif::NiControllerSequence>(root) != nullptr;
						}
						it = sequenceCounts.emplace(kfKey, count).first;
					}
					if (it->second == SIZE_MAX)
					{
						++missingKf;
						if (kfmErrors.size() < maxErrorSamples)
							kfmErrors.push_back(std::format("{}: missing {}", loc.key, kfKey));
					}
					else if (a.sequenceIndex >= it->second)
						++badIndex;
				}
			}
			catch (const std::exception& e)
			{
				++kfmFailed;
				if (kfmErrors.size() < maxErrorSamples)
					kfmErrors.push_back(std::format("{}: {}", loc.display(), e.what()));
			}
		}
		std::cout << std::format("\nkfm: parsed {}, failed {}, animations {}, missing kf {}, bad sequence index {}\n",
			kfmOk, kfmFailed, animations, missingKf, badIndex);
		for (const auto& [v, n] : kfmVersions)
			std::cout << std::format("  version {:<10} {}\n", v, n);
		for (const auto& [t, n] : transitionTypes)
			std::cout << std::format("  transition type {} x{}\n", t, n);
		for (const auto& e : kfmErrors)
			std::cout << "  " << e << '\n';
		for (const auto& s : unresolvedSamples)
			std::cout << "  unresolved: " << s << '\n';

		std::vector<std::pair<std::string, size_t>> folders(unresolvedFoundIn.begin(), unresolvedFoundIn.end());
		std::ranges::sort(folders, [](const auto& a, const auto& b) { return a.second > b.second; });
		std::cout << std::format("\nunresolved textures exist elsewhere in (top folders), nowhere: {}\n", unresolvedNowhere);
		for (size_t i = 0; i < folders.size() && i < 25; ++i)
			std::cout << std::format("  {:>5}  {}\n", folders[i].second, folders[i].first);
	}
	return 0;
}
