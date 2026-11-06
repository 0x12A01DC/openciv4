// The text layout against DirectWrite's own (the layout the overlay drew with before): widths,
// heights and the lines' baselines of the game's kinds of text.

#include "render/TextLayout.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace oc4::render;

namespace
{
	struct Case
	{
		std::wstring family, text;
		float size;
		bool bold;
		float maxWidth;
		// (a span of another style: its start and length, size, bold, italic; length 0: none)
		size_t start = 0, length = 0;
		float otherSize = 0;
		bool otherBold = false, otherItalic = false;
	};

	struct Reference
	{
		float width = 0, height = 0;
		std::vector<float> baselines;
	};

	Reference reference(IDWriteFactory* factory, const Case& c)
	{
		ComPtr<IDWriteTextFormat> format;
		factory->CreateTextFormat(c.family.c_str(), nullptr, c.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, c.size, L"", &format);
		ComPtr<IDWriteTextLayout> layout;
		factory->CreateTextLayout(c.text.c_str(), UINT32(c.text.size()), format.Get(), c.maxWidth, 4096, &layout);
		if (c.length)
		{
			const DWRITE_TEXT_RANGE range{ UINT32(c.start), UINT32(c.length) };
			layout->SetFontSize(c.otherSize, range);
			layout->SetFontWeight(c.otherBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, range);
			layout->SetFontStyle(c.otherItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, range);
		}
		DWRITE_TEXT_METRICS metrics{};
		layout->GetMetrics(&metrics);
		Reference r{ metrics.widthIncludingTrailingWhitespace, metrics.height };
		UINT32 count = 0;
		layout->GetLineMetrics(nullptr, 0, &count);
		std::vector<DWRITE_LINE_METRICS> lines(count);
		layout->GetLineMetrics(lines.data(), count, &count);
		float top = 0;
		for (const auto& l : lines)
		{
			r.baselines.push_back(top + l.baseline);
			top += l.height;
		}
		return r;
	}
}

int main()
{
	ComPtr<IDWriteFactory> factory;
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
		return 1;
	const std::wstring lorem = L"The city of Rome has grown to size 30. Its people demand more happiness, and the granary is full: "
							   L"a long sentence that wraps over several lines of the help text.";
	const std::vector<Case> cases = {
		{ L"Sylfaen", L"The Fog lingers near Tuareg. (x2)", 14, false, 4096 },
		{ L"Sylfaen", L"Commonwealth of Rome", 14, true, 4096 },
		{ L"Sylfaen", L"Waiting for other Civilizations...", 16, false, 4096 },
		{ L"Sylfaen", L"3562 +32 [Commonwealth of Rome]", 12, false, 4096 },
		{ L"Sylfaen", L"Ожидание других цивилизаций", 14, false, 4096 },
		{ L"Sylfaen", L"★ Rome", 14, true, 4096 },
		{ L"Sylfaen", L"Strength:\t70", 14, false, 4096 },
		{ L"Sylfaen", L"First line\nSecond line\n", 14, false, 4096 },
		{ L"Sylfaen", L"trailing   ", 14, false, 4096 },
		{ L"Sylfaen", lorem, 14, false, 250 },
		{ L"Sylfaen", lorem, 12, false, 180 },
		{ L"Sylfaen", L"Unbreakablewordthatislongerthantheline and more", 14, false, 120 },
		{ L"Sylfaen", L"Research: Future Tech 4 (73)", 22, false, 4096 },
		{ L"Trebuchet MS", L"Pacific Peaceful Soc. Rep. of Aborigines", 15, true, 4096 },
		{ L"Trebuchet MS", lorem, 15, true, 300 },
		{ L"Sylfaen", L"Production: Settler (3 turns)", 14, false, 4096, 12, 7, 22, true, false },
		{ L"Sylfaen", lorem, 14, false, 220, 30, 40, 16, false, true },
		{ L"Sylfaen", lorem, 12, false, 200, 0, 20, 22, true, true },
	};
	Fonts fonts;
	int failures = 0;
	for (const Case& c : cases)
	{
		const Reference want = reference(factory.Get(), c);
		std::vector<TextSpan> spans{ { 0, c.text.size(), TextStyle{ c.size, c.bold } } };
		if (c.length)
		{
			const TextStyle base{ c.size, c.bold }, other{ c.otherSize, c.otherBold, c.otherItalic };
			spans = { { 0, c.start, base }, { c.start, c.length, other }, { c.start + c.length, c.text.size() - c.start - c.length, base } };
			if (c.start == 0)
				spans.erase(spans.begin());
		}
		const LaidText got = layOut(fonts, c.family, c.text, spans, {}, c.maxWidth);
		std::vector<float> baselines;
		for (const auto& g : got.glyphs)
			if (baselines.empty() || g.baseline != baselines.back())
				baselines.push_back(g.baseline);
		const bool ok = std::abs(got.width - want.width) < 0.6f && std::abs(got.height - want.height) < 0.6f &&
			(baselines.empty() || std::abs(baselines[0] - want.baselines[0]) < 0.6f);
		bool linesOk = true;
		for (size_t k = 0; k < baselines.size() && k < want.baselines.size(); ++k)
			linesOk &= std::abs(baselines[k] - want.baselines[k]) < 0.6f;
		const size_t visibleLines = want.baselines.size() - (c.text.ends_with(L"\n") ? 1 : 0);
		linesOk &= baselines.size() == visibleLines || baselines.empty();
		if (!ok || !linesOk)
		{
			++failures;
			std::printf("FAIL %ls %g%s w%g: width %.2f (want %.2f), height %.2f (want %.2f), %zu lines (want %zu), first baseline %.2f (want %.2f)\n",
				c.family.c_str(), c.size, c.bold ? " bold" : "", c.maxWidth, got.width, want.width, got.height, want.height, baselines.size(), visibleLines,
				baselines.empty() ? 0.0f : baselines[0], want.baselines[0]);
		}
	}
	std::printf("%d of %zu cases differ\n", failures, cases.size());
	return failures ? 1 : 0;
}
