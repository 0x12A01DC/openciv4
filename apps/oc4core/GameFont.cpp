#include "GameFont.h"
#include "Diplomacy.h"

#include "Interface.h"

#include "CvGameCoreDLL.h"
#include "CvDllTranslator.h"
#include "CvGameTextMgr.h"
#include "CvGlobals.h"

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <format>
#include <unordered_map>
#include <vector>

namespace oc4::host
{
	namespace
	{
		std::unordered_map<wchar_t, std::wstring>& standIns()
		{
			static std::unordered_map<wchar_t, std::wstring> map;
			return map;
		}

		// [ICON_x] names without underscores ("GREAT_PEOPLE" and "GREATPEOPLE" alike).
		std::unordered_map<std::wstring, wchar_t>& icons()
		{
			static std::unordered_map<std::wstring, wchar_t> map;
			return map;
		}

		std::vector<std::pair<std::vector<int>, int>> g_fontGroups;

		std::wstring iconKey(std::wstring_view name)
		{
			std::wstring key;
			for (const wchar_t c : name)
				if (c != L'_')
					key += c;
			return key;
		}
	}

	void assignGameFont()
	{
		// (the DLL tells the symbols' codes back through setSymbolID)
		GAMETEXT.assignFontIds(kFirstGameFontChar, kGameFontRow);
		std::vector<std::vector<wchar>> rows;
		std::vector<int> rowCounts;
		GAMETEXT.getFontSymbols(rows, rowCounts);
		g_fontGroups.clear();
		for (size_t g = 0; g < rows.size() && g < rowCounts.size(); ++g)
			g_fontGroups.push_back({ std::vector<int>(rows[g].begin(), rows[g].end()), rowCounts[g] });

		auto& map = standIns();
		map.clear();
		const auto put = [&](const std::vector<wchar>& row, std::initializer_list<const wchar_t*> texts) {
			size_t i = 0;
			for (const wchar_t* t : texts)
				if (i < row.size())
					map[row[i++]] = t;
		};
		if (rows.size() > 0)
			put(rows[0], { L"\U0001F33E", L"\x2692", L"\U0001F4B0" }); // food, production, commerce
		if (rows.size() > 1)
			put(rows[1], { L"\U0001FA99", L"\U0001F9EA", L"\U0001F3AD", L"\U0001F441" }); // gold, research, culture, espionage
		auto& names = icons();
		names.clear();
		const auto name = [&](size_t row, std::initializer_list<const wchar_t*> keys) {
			size_t i = 0;
			for (const wchar_t* k : keys)
				if (row < rows.size() && i < rows[row].size())
					names[k] = rows[row][i++];
		};
		name(0, { L"FOOD", L"PRODUCTION", L"COMMERCE" });
		name(1, { L"GOLD", L"RESEARCH", L"CULTURE", L"ESPIONAGE" });
		static const wchar_t* const kSymbolNames[] = { L"HAPPY", L"UNHAPPY", L"HEALTHY", L"UNHEALTHY", L"BULLET", L"STRENGTH", L"MOVES",
			L"RELIGION", L"STAR", L"SILVERSTAR", L"TRADE", L"DEFENSE", L"GREATPEOPLE", L"BADGOLD", L"BADFOOD", L"EATENFOOD", L"GOLDENAGE",
			L"ANGRYPOP", L"OPENBORDERS", L"DEFENSIVEPACT", L"MAP", L"OCCUPATION", L"POWER" }; // FontSymbols' order
		for (int s = 0; s < int(std::size(kSymbolNames)) && s < MAX_NUM_SYMBOLS; ++s)
			names[kSymbolNames[s]] = wchar_t(Interface::symbolID(s));
		if (rows.size() > 2)
			for (size_t i = 0; i < rows[2].size(); ++i)
				map[rows[2][i]] = i % 2 ? L"\x2726" : L"\x271D"; // religions and corporations, their holy cities and headquarters
		if (rows.size() > 3)
			for (const wchar c : rows[3])
				map[c] = L""; // resources: their names are written out
		for (int s = 0; s < MAX_NUM_SYMBOLS; ++s)
		{
			static const wchar_t* const kSymbols[] = { L"\U0001F642", L"\U0001F641", L"\x2764", L"\x2623", L"\x2022", L"\x2694", L"\U0001F463",
				L"\x271D", L"\x2605", L"\x2606", L"\x21C4", L"\U0001F6E1", L"\U0001F464", L"\U0001F4B8", L"\U0001F342", L"\U0001F37D", L"\x2600",
				L"\U0001F620", L"\x21D4", L"\U0001F91D", L"\U0001F5FA", L"\x26D3", L"\x26A1" };
			map[wchar_t(Interface::symbolID(s))] = s < int(std::size(kSymbols)) ? kSymbols[s] : L"";
		}
	}

	const std::vector<std::pair<std::vector<int>, int>>& fontSymbolGroups() { return g_fontGroups; }

	bool resolveTextTag(std::wstring_view tag, std::wstring& out)
	{
		if (tag.starts_with(L"ICON_"))
		{
			const auto it = icons().find(iconKey(tag.substr(5)));
			if (it == icons().end())
				return false;
			out.assign(1, it->second);
			return true;
		}
		if (tag.starts_with(L"COLOR_"))
		{
			std::string type;
			for (const wchar_t c : tag)
				type += static_cast<char>(c);
			const int color = GC.getInfoTypeForString(type.c_str(), true);
			if (color < 0 || color >= GC.getNumColorInfos())
				return false;
			const NiColorA& c = GC.getColorInfo(static_cast<ColorTypes>(color)).getColor();
			out = std::format(L"<color={},{},{},{}>", int(c.r * 255), int(c.g * 255), int(c.b * 255), int(c.a * 255));
			return true;
		}
		// The talk's players, as the original engine has the game core fill them in
		// (CvDllTranslator): [OUR_...] the leader speaking, [CT_...] the one spoken to (the
		// active player); ":n" a grammatical form, counted from 1. Outside a talk [OUR_...]
		// stays as it is.
		const bool ours = tag.starts_with(L"OUR_");
		if (ours || tag.starts_with(L"CT_"))
		{
			std::wstring key(tag);
			int form = 0;
			if (const size_t colon = key.find(L':'); colon != std::wstring::npos)
			{
				form = std::max(std::wcstol(key.c_str() + colon + 1, nullptr, 10) - 1, 0L);
				key.resize(colon);
			}
			int player = ours ? gDLL->getDiplomacyPlayer() : int(GC.getGame().getActivePlayer());
			// (a leader's text read outside its talk: the last leader talked to)
			if (ours && (player < 0 || player >= GC.getMAX_PLAYERS()))
				player = g_lastDiplomacyPlayer;
			if (player < 0 || player >= GC.getMAX_PLAYERS())
				return false;
			CvWString replaced;
			const CvWString wanted((L"[" + key).c_str());
			// (the game core asks who the leader is: this one, while it fills the tag)
			g_diplomacyPlayerOverride = player;
			const bool done = ours ? CvDllTranslator::replaceOur(wanted, form, replaced) : CvDllTranslator::replaceCt(wanted, form, replaced);
			g_diplomacyPlayerOverride = -1;
			if (!done)
				return false;
			out = replaced.GetCString();
			return true;
		}
		return false;
	}

	namespace
	{
		// The text tags left in a text ("[PARAGRAPH:1]", "[ICON_GOLD]", "[COLOR_WARNING_TEXT]" in a
		// script's own strings, not through the translator), resolved as the translator does.
		std::wstring expandTags(const wchar_t* text)
		{
			std::wstring out;
			for (const wchar_t* c = text ? text : L""; *c; ++c)
			{
				if (*c == L'[')
				{
					const wchar_t* end = c + 1;
					while (*end && (std::iswupper(*end) || std::iswdigit(*end) || *end == L'_' || *end == L':'))
						++end;
					if (*end == L']' && end > c + 1 && std::iswupper(c[1]))
					{
						const std::wstring tag(c + 1, end);
						std::wstring resolved;
						bool done = true;
						if (tag.starts_with(L"PARAGRAPH:"))
							resolved.assign(tag.ends_with(L'2') ? 2 : 1, L'\n');
						else if (tag == L"NEWLINE")
							resolved = L"\n";
						else if (tag == L"TAB")
							resolved = L"\t";
						else if (tag == L"SPACE")
							resolved = L" ";
						else if (tag == L"COLOR_REVERT")
							resolved = L"</color>";
						else
							done = resolveTextTag(tag, resolved);
						if (done)
						{
							out += resolved;
							c = end;
							continue;
						}
					}
				}
				out += *c;
			}
			return out;
		}

		std::string convert(const wchar_t* raw, bool keepMarkup)
		{
			const std::wstring expanded = raw && std::wcschr(raw, L'[') ? expandTags(raw) : std::wstring();
			const wchar_t* text = raw && std::wcschr(raw, L'[') ? expanded.c_str() : raw;
			const auto& map = standIns();
			// The screens' markup (<color=...>, <font=2>, </link>, ...) goes; other text in angle
			// brackets ("Press <ENTER> to end turn") stays.
			const auto markup = [](const wchar_t* c) -> const wchar_t* {
				const wchar_t* name = c + 1 + (c[1] == L'/');
				const wchar_t* end = name;
				while (std::iswalpha(*end))
					++end;
				std::wstring tag(name, end);
				for (wchar_t& ch : tag)
					ch = wchar_t(std::towlower(ch));
				static const wchar_t* const kTags[] = { L"color", L"font", L"link", L"b", L"i", L"u", L"img", L"shadow", L"outline" };
				if (std::ranges::find_if(kTags, [&](const wchar_t* t) { return tag == t; }) == std::end(kTags) || (*end != L'>' && *end != L'='))
					return nullptr;
				const wchar_t* close = std::wcschr(end, L'>');
				return close;
			};
			std::wstring out;
			for (const wchar_t* c = text ? text : L""; *c; ++c)
			{
				if (const wchar_t* close = *c == L'<' ? markup(c) : nullptr)
				{
					if (keepMarkup)
						out.append(c, close + 1);
					c = close;
				}
				else if (!keepMarkup && *c == L'&' && (std::wcsncmp(c, L"&lt;", 4) == 0 || std::wcsncmp(c, L"&gt;", 4) == 0))
				{
					out += c[1] == L'l' ? L'<' : L'>'; // (the markup's escapes; kept with the markup)
					c += 3;
				}
				else if (!keepMarkup && std::wcsncmp(c, L"&amp;", 5) == 0)
				{
					out += L'&';
					c += 4;
				}
				// (the font's icons: the window draws them from the font with the markup, the
				// stand-ins are for plain text)
				else if (keepMarkup && *c >= kFirstGameFontChar && *c < kFirstGameFontChar + 200 * kGameFontRow)
					out += std::format(L"<img=font:{}>", int(*c));
				else if (const auto it = map.find(*c); it != map.end())
					out += it->second;
				else if (*c >= kFirstGameFontChar && *c < kFirstGameFontChar + 200 * kGameFontRow)
					continue; // a glyph of the font we know nothing about
				else
					out += *c;
			}
			while (!out.empty() && (out.back() == L' ' || out.back() == L'\n'))
				out.pop_back();
			if (out.empty())
				return {};
			const int n = WideCharToMultiByte(CP_UTF8, 0, out.c_str(), int(out.size()), nullptr, 0, nullptr, nullptr);
			std::string s(size_t(n), '\0');
			WideCharToMultiByte(CP_UTF8, 0, out.c_str(), int(out.size()), s.data(), n, nullptr, nullptr);
			return s;
		}
	}

	std::string gameText(const wchar_t* text) { return convert(text, false); }
	std::string gameMarkup(const wchar_t* text) { return convert(text, true); }
}
