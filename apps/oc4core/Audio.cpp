#include "Audio.h"

#include "Xml.h"

#include "CvGameCoreDLL.h"

#include <cstdio>
#include <cwchar>

namespace oc4::host
{
	namespace
	{
		std::string narrow(const std::wstring& s)
		{
			std::string out;
			out.reserve(s.size());
			for (const wchar_t c : s)
				out.push_back(static_cast<char>(c));
			return out;
		}

		std::string textOf(const XmlNode& n, std::string_view tag)
		{
			const XmlNode* c = n.firstChildElement(tag);
			return c ? narrow(c->text) : std::string();
		}

		int intOf(const XmlNode& n, std::string_view tag, int otherwise)
		{
			const XmlNode* c = n.firstChildElement(tag);
			return c && !c->text.empty() ? int(std::wcstol(c->text.c_str(), nullptr, 10)) : otherwise;
		}
	}

	void AudioScripts::load()
	{
		const auto parse = [&](const char* file) -> std::unique_ptr<XmlNode> {
			const auto loc = m_assets.locate(file);
			if (!loc)
			{
				std::fprintf(stderr, "[oc4core] audio XML not found: %s\n", file);
				return nullptr;
			}
			std::string error;
			auto doc = parseXml(m_assets.read(*loc), error);
			if (!doc)
				std::fprintf(stderr, "[oc4core] xml parse error in %s: %s\n", loc->display().c_str(), error.c_str());
			return doc;
		};
		const auto elements = [](const XmlNode& n, auto&& each) {
			for (const auto& c : n.children)
				if (c->type == XmlNode::Type::Element)
					each(*c);
		};

		static constexpr const char* files[TableCount] = {
			"xml/audio/audio2dscripts.xml",
			"xml/audio/audio3dscripts.xml",
			"xml/audio/audiosoundscapescripts.xml",
		};
		for (int t = 0; t < TableCount; ++t)
		{
			const auto doc = parse(files[t]);
			if (!doc)
				continue;
			// <Script2DSounds><Script2DSound><ScriptID>AS2D_ERROR</ScriptID>...
			elements(*doc, [&](const XmlNode& root) {
				int next = 0;
				elements(root, [&](const XmlNode& node) {
					const std::string id = textOf(node, "ScriptID");
					if (!id.empty())
						m_tables[t].try_emplace(id, next);
					if (t < Soundscape)
					{
						Script s;
						s.id = id;
						s.sound = textOf(node, "SoundID");
						const std::string type = textOf(node, "SoundType");
						s.kind = type == "GAME_MUSIC" ? Kind::Music : type == "GAME_SPEECH" ? Kind::Speech : type == "GAME_AMBIENCE" ? Kind::Ambience
							: type == "GAME_INTERFACE" ? Kind::Interface : Kind::Sfx;
						s.minVolume = intOf(node, "iMinVolume", 100), s.maxVolume = intOf(node, "iMaxVolume", s.minVolume);
						s.pitchDown = intOf(node, "iPitchChangeDown", 0), s.pitchUp = intOf(node, "iPitchChangeUp", 0);
						s.looping = intOf(node, "bLooping", 0) != 0;
						s.notPlayPercent = intOf(node, "iNotPlayPercent", 0);
						s.fullVolumeDistance = intOf(node, "iMinDistanceForMaxVolume", 0);
						s.cutoffDistance = intOf(node, "iMaxCutoffDistance", 0);
						s.minDelay = intOf(node, "iMinTimeDelay", 0), s.maxDelay = intOf(node, "iMaxTimeDelay", s.minDelay);
						s.minListenerDistance = intOf(node, "iMinDistanceFromListener", 0);
						s.maxListenerDistance = intOf(node, "iMaxDistanceFromListener", s.minListenerDistance);
						m_scripts[size_t(t)].push_back(std::move(s));
					}
					else
					{
						// (after the 2D and 3D tables: its sounds are theirs)
						Scape s;
						s.id = id;
						s.minVolume = intOf(node, "iMinVolume", 100), s.maxVolume = intOf(node, "iMaxVolume", s.minVolume);
						elements(node, [&](const XmlNode& e) {
							if (e.name != "SoundscapeElement")
								return;
							const Table table = textOf(e, "ScriptType") == "2D" ? Script2D : Script3D;
							const auto it = m_tables[table].find(textOf(e, "ScriptID"));
							s.elements.emplace_back(table, it != m_tables[table].end() ? it->second : -1);
						});
						m_soundscapes.push_back(std::move(s));
					}
					++next;
				});
			});
		}
		// <AudioDefines><SoundDatas><SoundData><SoundID>SND_ERROR</SoundID><Filename>Sounds/Error</Filename>
		if (const auto doc = parse("xml/audio/audiodefines.xml"))
			elements(*doc, [&](const XmlNode& root) {
				if (const XmlNode* datas = root.firstChildElement("SoundDatas"))
					elements(*datas, [&](const XmlNode& d) { m_files.try_emplace(textOf(d, "SoundID"), textOf(d, "Filename")); });
			});
		// <Civ4TerrainInfos><TerrainInfos><TerrainInfo><Type>TERRAIN_GRASS</Type>...
		// <WorldSoundscapeAudioScript>ASSS_GRASSLAND_SELECT_AMB</WorldSoundscapeAudioScript> (features, improvements alike)
		for (const char* file : { "xml/terrain/civ4terraininfos.xml", "xml/terrain/civ4featureinfos.xml", "xml/terrain/civ4improvementinfos.xml" })
			if (const auto doc = parse(file))
				elements(*doc, [&](const XmlNode& root) {
					elements(root, [&](const XmlNode& infos) {
						elements(infos, [&](const XmlNode& info) {
							if (std::string tag = textOf(info, "WorldSoundscapeAudioScript"); !tag.empty())
								m_worldSoundscapes.try_emplace(textOf(info, "Type"), std::move(tag));
						});
					});
				});
		// <Civ4EraInfos><EraInfos><EraInfo><Type>ERA_ANCIENT</Type>...<EraInfoSoundtracks><EraInfoSoundtrack>
		if (const auto doc = parse("xml/gameinfo/civ4erainfos.xml"))
			elements(*doc, [&](const XmlNode& root) {
				if (const XmlNode* eras = root.firstChildElement("EraInfos"))
					elements(*eras, [&](const XmlNode& era) {
						Soundtracks s;
						s.space = intOf(era, "iSoundtrackSpace", 0);
						s.firstFirst = intOf(era, "bFirstSoundtrackFirst", 0) != 0;
						if (const XmlNode* list = era.firstChildElement("EraInfoSoundtracks"))
							elements(*list, [&](const XmlNode& t) { s.scripts.push_back(narrow(t.text)); });
						m_soundtracks.try_emplace(textOf(era, "Type"), std::move(s));
					});
			});
	}

	int AudioScripts::index(std::string_view tag, int scriptType)
	{
		std::call_once(m_loaded, [this] { load(); });
		const auto find = [&](int table) {
			const auto it = m_tables[table].find(std::string(tag));
			return it != m_tables[table].end() ? it->second : -1;
		};
		switch (scriptType)
		{
		case AUDIOTAG_2DSCRIPT: return find(Script2D);
		case AUDIOTAG_3DSCRIPT: return find(Script3D);
		case AUDIOTAG_SOUNDSCAPE: return find(Soundscape);
		case AUDIOTAG_NONE:
			for (int t = 0; t < TableCount; ++t)
				if (const int i = find(t); i >= 0)
					return i;
			return -1;
		default: return -1;
		}
	}

	const AudioScripts::Script* AudioScripts::script(Table table, int index)
	{
		std::call_once(m_loaded, [this] { load(); });
		if (table >= Soundscape || index < 0 || size_t(index) >= m_scripts[size_t(table)].size())
			return nullptr;
		return &m_scripts[size_t(table)][size_t(index)];
	}

	std::string AudioScripts::soundFile(std::string_view sound)
	{
		std::call_once(m_loaded, [this] { load(); });
		const auto it = m_files.find(std::string(sound));
		return it != m_files.end() ? it->second : std::string();
	}

	const AudioScripts::Scape* AudioScripts::soundscape(int index)
	{
		std::call_once(m_loaded, [this] { load(); });
		return index >= 0 && size_t(index) < m_soundscapes.size() ? &m_soundscapes[size_t(index)] : nullptr;
	}

	std::string AudioScripts::worldSoundscape(std::string_view infoType)
	{
		std::call_once(m_loaded, [this] { load(); });
		const auto it = m_worldSoundscapes.find(std::string(infoType));
		return it != m_worldSoundscapes.end() ? it->second : std::string();
	}

	const AudioScripts::Soundtracks* AudioScripts::soundtracks(std::string_view eraType)
	{
		std::call_once(m_loaded, [this] { load(); });
		const auto it = m_soundtracks.find(std::string(eraType));
		return it != m_soundtracks.end() ? &it->second : nullptr;
	}
}
