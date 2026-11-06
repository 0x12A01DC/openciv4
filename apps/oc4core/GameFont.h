#pragma once

// The game font, as far as the DLL sees it: the original engine numbers the font's glyphs
// when it loads it (CvGameTextMgr::assignFontIds gives the yields, commerces, religions,
// corporations and resources their characters from the first code on, a row of 25 each)
// and places the symbols (FontSymbols: happy, strength, ...) after them. The DLL's texts
// carry these characters; gameText turns them into plain text with Unicode stand-ins,
// gameMarkup into <img=font:code> for the window to draw from the font.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oc4::host
{
	constexpr int kFirstGameFontChar = 8483;
	constexpr int kGameFontRow = 25;

	// After the XML is loaded, as the original engine does when it loads the font.
	void assignGameFont();
	// The DLL's groups of the font's icons (CvGameTextMgr::getFontSymbols): each group's
	// characters and the rows of 25 cells it takes in the font.
	const std::vector<std::pair<std::vector<int>, int>>& fontSymbolGroups();

	// UTF-8 without the markup (<color=...>, <link=...>); game font characters as Unicode.
	std::string gameText(const wchar_t* text);
	// The same with the screens' markup kept (<font=2>, <color=...>, <link=...>, ...): for the
	// window to draw as the screens mean it.
	std::string gameMarkup(const wchar_t* text);

	// The text tags that need the game: [ICON_x] (the font's characters, once it is
	// assigned) and [COLOR_x] (CvColorInfo, as <color=r,g,b,a> the way the DLL writes it).
	bool resolveTextTag(std::wstring_view tag, std::wstring& out);
}
