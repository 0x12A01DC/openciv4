#pragma once

// Text laid out in lines as DirectWrite's default layout does it: wrapped at spaces and hyphens
// (a word longer than the line broken where it overflows), the lines as high as the tallest of
// their fonts and pictures, trailing spaces hanging past the width.

#include "render/Font.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oc4::render
{
	struct TextStyle
	{
		float size = 14;
		bool bold = false, italic = false, underline = false;
		uint32_t rgba = 0; // 0: the colour the text is drawn in
	};

	struct TextSpan
	{
		size_t start = 0, length = 0;
		TextStyle style;
	};

	// A picture standing in for the character at `at` (a line's height and baseline make room for it: 0.82 of it above).
	struct TextInline
	{
		size_t at = 0;
		float width = 0, height = 0;
	};

	struct LaidText
	{
		struct Glyph
		{
			int face = 0;
			uint16_t index = 0;
			float x = 0, baseline = 0, size = 0;
			uint32_t rgba = 0;
		};
		struct Underline
		{
			float x0 = 0, x1 = 0, y = 0, thickness = 1;
			uint32_t rgba = 0;
		};
		struct Picture
		{
			size_t inlineIndex = 0;
			float x = 0, y = 0; // top left
		};
		std::vector<Glyph> glyphs;
		std::vector<Underline> underlines;
		std::vector<Picture> pictures;
		float width = 0, height = 0; // (the width with the lines' trailing spaces)
	};

	// spans cover the text (characters outside them have the first span's style, or a default one).
	LaidText layOut(Fonts& fonts, const std::wstring& family, std::wstring_view text, const std::vector<TextSpan>& spans,
		const std::vector<TextInline>& inlines, float maxWidth);
}
