#pragma once

// Gamebryo KFM (NiKFMTool) animation set: which model, which .kf file and
// sequence index plays for each event code, and how to transition between them.
// Binary versions 1.2.4b and 2.0.0.0b are supported (Civ4 ships almost only 2.0.0.0b).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace oc4::nif
{
	enum class KfmTransitionType : uint32_t
	{
		Blend = 0,
		Morph = 1,
		CrossFade = 2,
		Chain = 3,
		DefaultSync = 4,
		DefaultNonSync = 5,
	};

	struct KfmTransition
	{
		uint32_t target = 0; // event code of the destination animation
		KfmTransitionType type = KfmTransitionType::DefaultNonSync;
		float duration = 0;  // only stored for non-default types
		struct BlendPair
		{
			std::string startKey, targetKey; // text keys, e.g. "start eventcode=6"
		};
		std::vector<BlendPair> blendPairs;
		struct ChainStep
		{
			uint32_t eventCode = 0;
			float duration = 0;
		};
		std::vector<ChainStep> chain;

		bool isDefault() const { return type == KfmTransitionType::DefaultSync || type == KfmTransitionType::DefaultNonSync; }
	};

	struct KfmAnimation
	{
		uint32_t eventCode = 0;
		std::string name; // 1.x only
		std::string kfFile; // relative to the .kfm
		uint32_t sequenceIndex = 0;
		std::vector<KfmTransition> transitions;
	};

	struct KfmFile
	{
		std::string version; // e.g. "2.0.0.0b"
		std::string nifFile;
		std::string masterNode; // accumulation root, e.g. "MD"
		float defaultSyncDuration = 0;
		float defaultNonSyncDuration = 0;
		std::vector<KfmAnimation> animations;

		const KfmAnimation* find(uint32_t eventCode) const;
	};

	// Throws ParseError on malformed or unsupported (text) files.
	KfmFile parseKfm(std::span<const std::byte> data);
}
