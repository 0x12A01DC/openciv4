#include "nif/KfmFile.h"

#include "core/BinaryReader.h"

#include <format>

namespace oc4::nif
{
	const KfmAnimation* KfmFile::find(uint32_t eventCode) const
	{
		for (const auto& a : animations)
			if (a.eventCode == eventCode)
				return &a;
		return nullptr;
	}

	KfmFile parseKfm(std::span<const std::byte> data)
	{
		BinaryReader r(data);
		const std::string header = r.readLine();
		constexpr std::string_view kPrefix = ";Gamebryo KFM File Version ";
		if (!header.starts_with(kPrefix))
			r.fail("not a KFM file");

		KfmFile kfm;
		kfm.version = header.substr(kPrefix.size());
		const bool v2 = kfm.version == "2.0.0.0b";
		if (!v2 && kfm.version != "1.2.4b")
			r.fail(std::format("unsupported KFM version {}", kfm.version));

		if (v2)
			(void)r.u8(); // always 1
		kfm.nifFile = r.readSizedString(1024);
		kfm.masterNode = r.readSizedString(1024);
		(void)r.u32(); // unknown, 1
		(void)r.u32(); // unknown, 0 or 2
		kfm.defaultSyncDuration = r.f32();
		kfm.defaultNonSyncDuration = r.f32();

		const uint32_t numAnimations = r.u32();
		r.requireArray(numAnimations, 16);
		kfm.animations.resize(numAnimations);
		for (auto& a : kfm.animations)
		{
			a.eventCode = r.u32();
			if (!v2)
				a.name = r.readSizedString(1024);
			a.kfFile = r.readSizedString(1024);
			a.sequenceIndex = r.u32();
			const uint32_t numTransitions = r.u32();
			r.requireArray(numTransitions, 8);
			a.transitions.resize(numTransitions);
			for (auto& t : a.transitions)
			{
				t.target = r.u32();
				const uint32_t type = r.u32();
				if (type > 5)
					r.fail(std::format("unknown KFM transition type {}", type));
				t.type = static_cast<KfmTransitionType>(type);
				if (t.isDefault())
					continue;
				t.duration = r.f32();
				const uint32_t numPairs = r.u32();
				r.requireArray(numPairs, 8);
				t.blendPairs.resize(numPairs);
				for (auto& p : t.blendPairs)
				{
					p.startKey = r.readSizedString(1024);
					p.targetKey = r.readSizedString(1024);
				}
				const uint32_t numChain = r.u32();
				r.requireArray(numChain, 8);
				t.chain.resize(numChain);
				for (auto& c : t.chain)
				{
					c.eventCode = r.u32();
					c.duration = r.f32();
				}
			}
		}
		(void)r.u32(); // trailing, 0
		if (!r.atEnd())
			r.fail(std::format("{} trailing bytes", r.remaining()));
		return kfm;
	}
}
