// animscan: binds every animation of every KFM in the install to its model and
// samples it, reporting channel coverage and numerical problems. --mod adds a mod's Assets
// folder over the install; --leaderheads checks only the leaders' heads, one line each (the
// actions their event codes have, their skin shaders, the textures they cannot find);
// --events A-B lists the KFMs' animations whose event codes (low 16 bits) are in that range.
//   animscan --civ4 <Civ4 root> [--mod <Assets folder>] [--leaderheads] [--events A-B] [--errors N]

#include "anim/Animation.h"
#include "core/AssetResolver.h"
#include "core/FileUtil.h"
#include "nif/KfmFile.h"
#include "nif/NifBlocks.h"
#include "nif/NifFile.h"
#include "scene/NifScene.h"

#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace oc4;

namespace
{
	bool finite(const Affine& a)
	{
		for (const auto& row : a.rotation.m)
			for (const float v : row)
				if (!std::isfinite(v))
					return false;
		return std::isfinite(a.translation.x) && std::isfinite(a.translation.y) && std::isfinite(a.translation.z) && std::isfinite(a.scale);
	}
}

int wmain(int argc, wchar_t** argv)
{
	std::optional<AssetResolver> resolver;
	std::optional<std::filesystem::path> civ4, mod;
	bool leaderheads = false;
	unsigned eventsFrom = 0, eventsTo = 0;
	size_t maxSamples = 10;
	for (int i = 1; i < argc; ++i)
	{
		const std::wstring a = argv[i];
		if (a == L"--civ4" && i + 1 < argc)
			civ4 = argv[++i];
		else if (a == L"--mod" && i + 1 < argc)
			mod = argv[++i];
		else if (a == L"--leaderheads")
			leaderheads = true;
		else if (a == L"--events" && i + 1 < argc)
			std::swscanf(argv[++i], L"%u-%u", &eventsFrom, &eventsTo);
		else if (a == L"--errors" && i + 1 < argc)
			maxSamples = std::stoul(argv[++i]);
	}
	if (civ4)
		resolver = AssetResolver::forInstall(*civ4, mod);
	if (!resolver)
	{
		std::cerr << "usage: animscan --civ4 <Civ4 root> [--mod <Assets folder>] [--leaderheads] [--errors N]\n";
		return 2;
	}

	size_t kfms = 0, modelsMissing = 0, animations = 0, fullyBound = 0, empty = 0, missingKf = 0, nonFinite = 0;
	size_t channels = 0, unbound = 0, visibility = 0, material = 0;
	std::map<std::string, size_t> otherChannelTypes; // interpolators we do not evaluate yet
	std::map<std::string, size_t> unboundTargets;    // controller type of unbound channels
	std::vector<std::string> samples;
	std::map<std::string, std::shared_ptr<const nif::File>> kfCache;

	const auto t0 = std::chrono::steady_clock::now();
	for (const auto& loc : resolver->list(".kfm"))
	{
		nif::KfmFile kfm;
		try
		{
			kfm = nif::parseKfm(resolver->read(loc));
		}
		catch (const std::exception&)
		{
			continue; // text KFMs (1.0) are reported by nifscan
		}
		const std::string folder = loc.key.substr(0, loc.key.rfind('/') + 1);
		if (leaderheads && !folder.starts_with("art/leaderheads/"))
			continue;
		if (eventsTo)
		{
			for (const auto& a : kfm.animations)
				if ((a.eventCode & 0xFFFF) >= eventsFrom && (a.eventCode & 0xFFFF) <= eventsTo)
					std::cout << std::format("{} {} {}\n", loc.key, a.eventCode & 0xFFFF, a.kfFile);
			continue;
		}
		++kfms;
		const auto modelLoc = resolver->locate(folder + kfm.nifFile);
		if (!modelLoc)
		{
			++modelsMissing;
			if (leaderheads)
				std::cout << std::format("{}: no model {}\n", loc.key, kfm.nifFile);
			continue;
		}
		scene::Model model;
		try
		{
			model = scene::buildModel(nif::parseFile(resolver->read(*modelLoc)));
		}
		catch (const std::exception& e)
		{
			std::cout << std::format("{}: {}\n", modelLoc->key, e.what());
			continue;
		}
		if (leaderheads)
		{
			// The actions it has (its event codes' low 16 bits), its skin shaders, its missing textures.
			std::string actions, shaders, missing;
			bool have[8]{};
			for (const auto& a : kfm.animations)
			{
				actions += std::format(" {}", a.eventCode & 0xFFFF);
				if ((a.eventCode & 0xFFFF) < 8)
					have[a.eventCode & 0xFFFF] = true;
			}
			for (const auto& item : model.items)
			{
				if (!item.material.shaderName.empty() && shaders.find(item.material.shaderName) == std::string::npos)
					shaders += " " + item.material.shaderName;
				if (!item.material.baseTexture.empty() && !resolver->locateTexture(*modelLoc, item.material.baseTexture))
					missing += " " + item.material.baseTexture;
			}
			std::string lacks;
			const bool background = kfm.animations.size() <= 1;
			for (int k = 0; k < 8 && !background; ++k)
				if (!have[k])
					lacks += std::format(" {}", k);
			std::cout << std::format("{}: actions{}{}{}{}\n", loc.key, actions, lacks.empty() ? "" : ", lacks" + lacks,
				shaders.empty() ? "" : ", shaders" + shaders, missing.empty() ? "" : ", missing" + missing);
		}

		for (const auto& a : kfm.animations)
		{
			++animations;
			const std::string kfKey = makeAssetKey(folder + a.kfFile);
			auto& kf = kfCache[kfKey];
			if (!kf)
				if (const auto kfLoc = resolver->locate(kfKey))
					kf = std::make_shared<nif::File>(nif::parseFile(resolver->read(*kfLoc)));
			if (!kf)
			{
				++missingKf;
				continue;
			}

			// Channel types that are neither transform nor visibility interpolators,
			// keyed by interpolator / controller / property / interpolator id.
			for (const nif::Ref root : kf->roots)
				if (const auto* seq = kf->get<nif::NiControllerSequence>(root))
					for (const auto& b : seq->controlledBlocks)
						if (const auto* interp = kf->get<nif::NiObject>(b.interpolator);
							interp && !dynamic_cast<const nif::NiTransformInterpolator*>(interp) &&
							!dynamic_cast<const nif::NiBSplineCompTransformInterpolator*>(interp) &&
							!dynamic_cast<const nif::NiBoolInterpolator*>(interp))
						{
							std::string ctrl, prop, iid, cid;
							if (const auto* pal = kf->get<nif::NiStringPalette>(b.stringPalette != nif::kNoRef ? b.stringPalette : seq->stringPalette))
							{
								ctrl = std::string(pal->at(b.controllerTypeOffset));
								prop = std::string(pal->at(b.propertyTypeOffset));
								iid = std::string(pal->at(b.interpolatorIdOffset));
								cid = std::string(pal->at(b.controllerIdOffset));
							}
							otherChannelTypes[std::format("{} | {} | {} | iid={} cid={}", *interp->typeName, ctrl, prop, iid, cid)]++;
						}

			const anim::Sequence seq = anim::Sequence::bind(kf, a.sequenceIndex, model);
			channels += seq.channelCount();
			visibility += seq.visibilityChannels();
			material += seq.materialChannels();
			unbound += seq.unboundChannels();
			if (seq.channelCount() == 0)
			{
				++empty;
				continue;
			}
			if (seq.unboundChannels() == 0)
				++fullyBound;
			else
			{
				for (const auto& n : seq.unboundNodes())
					unboundTargets[n.substr(n.find(" ("))]++;
				if (samples.size() < maxSamples)
					samples.push_back(std::format("{} {}: {} unbound of {}, e.g. {}", loc.key, a.kfFile, seq.unboundChannels(),
						seq.unboundChannels() + seq.channelCount(), seq.unboundNodes().front()));
			}

			bool bad = false;
			for (int step = 0; step <= 8 && !bad; ++step)
			{
				anim::Pose pose = anim::Pose::bind(model);
				seq.apply(seq.duration() * float(step) / 8.0f, pose);
				for (const auto& m : pose.materials)
					if (anim::isSet(m.alpha) && !std::isfinite(m.alpha))
						bad = true;
				for (const auto& w : model.worldTransforms(pose.locals))
					if (!finite(w))
					{
						bad = true;
						break;
					}
			}
			if (bad)
			{
				++nonFinite;
				if (samples.size() < maxSamples)
					samples.push_back(std::format("{} {}: non-finite transform", loc.key, a.kfFile));
			}
		}
	}
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

	std::cout << std::format("kfm: {}  (model missing: {})  animations: {}  missing kf: {}  time: {:.1f}s\n", kfms, modelsMissing, animations, missingKf, seconds);
	std::cout << std::format("bound: {} fully, {} partially, {} with no evaluated channels; non-finite poses: {}\n",
		fullyBound, animations - missingKf - fullyBound - empty, empty, nonFinite);
	std::cout << std::format("evaluated channels: {} bound ({} visibility, {} material), {} unbound ({:.2f}%)\n", channels, visibility, material, unbound,
		channels + unbound ? 100.0 * double(unbound) / double(channels + unbound) : 0.0);
	std::cout << "non-transform, non-visibility channels in KF files:\n";
	for (const auto& [t, n] : otherChannelTypes)
		std::cout << std::format("  {:<40} {}\n", t, n);
	std::cout << "unbound channels by controller:\n";
	for (const auto& [t, n] : unboundTargets)
		std::cout << std::format("  {:<40} {}\n", t, n);
	for (const auto& s : samples)
		std::cout << "  " << s << '\n';
	return 0;
}
