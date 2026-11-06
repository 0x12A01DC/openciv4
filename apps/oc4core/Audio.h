#pragma once

// Audio script tables (XML/Audio/*Scripts.xml). The DLL stores script ids from
// getAudioTagIndex in its infos; the id is the script's position in its table,
// so the tables are needed even without an audio device. To play one there is the
// script's sound (AudioDefines.xml: its file), how loud and how far it carries.

#include "core/AssetResolver.h"

#include <array>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oc4::host
{
	class AudioScripts
	{
	public:
		explicit AudioScripts(const AssetResolver& assets) : m_assets(assets) {}

		// AUDIOTAG_2DSCRIPT, AUDIOTAG_3DSCRIPT or AUDIOTAG_SOUNDSCAPE; -1 searches all three.
		int index(std::string_view tag, int scriptType);

		enum Table
		{
			Script2D,
			Script3D,
			Soundscape,
			TableCount
		};
		enum class Kind : uint8_t
		{
			Sfx, Music, Speech, Ambience, Interface
		};
		struct Script
		{
			std::string id, sound; // AS2D_ERROR, SND_ERROR
			Kind kind = Kind::Sfx;
			int minVolume = 100, maxVolume = 100; // percent
			int pitchDown = 0, pitchUp = 0;       // thousandths of a percent
			bool looping = false;
			int notPlayPercent = 0;
			// 3D: full volume as far as this, nothing past the cutoff (world units)
			int fullVolumeDistance = 0, cutoffDistance = 0;
			// In a soundscape: the pause before it plays again (ms), and (3D) how far from the
			// listener it sounds (world units).
			int minDelay = 0, maxDelay = 0, minListenerDistance = 0, maxListenerDistance = 0;
		};
		// A soundscape (AudioSoundscapeScripts.xml): its sounds together, a 2D bed and 3D
		// sounds now and then.
		struct Scape
		{
			std::string id;
			int minVolume = 100, maxVolume = 100;
			std::vector<std::pair<Table, int>> elements; // (a 2D or 3D script; -1 if unknown)
		};
		const Scape* soundscape(int index);
		// The soundscape tag of a terrain, feature or improvement type (their XML's
		// WorldSoundscapeAudioScript); empty if none.
		std::string worldSoundscape(std::string_view infoType);
		// A 2D or 3D script by its index (nullptr: none).
		const Script* script(Table table, int index);
		// The asset path of a sound without its extension ("Sounds/Error"); empty if unknown.
		std::string soundFile(std::string_view sound);
		// The tags of each era's soundtracks (CIV4EraInfos.xml), by era type ("ERA_ANCIENT").
		struct Soundtracks
		{
			std::vector<std::string> scripts;
			int space = 0; // the era's iSoundtrackSpace
			bool firstFirst = false;
		};
		const Soundtracks* soundtracks(std::string_view eraType);

	private:
		void load();

		const AssetResolver& m_assets;
		std::once_flag m_loaded;
		std::array<std::unordered_map<std::string, int>, TableCount> m_tables;
		std::array<std::vector<Script>, 2> m_scripts;
		std::unordered_map<std::string, std::string> m_files;
		std::unordered_map<std::string, Soundtracks> m_soundtracks;
		std::vector<Scape> m_soundscapes;
		std::unordered_map<std::string, std::string> m_worldSoundscapes;
	};
}
