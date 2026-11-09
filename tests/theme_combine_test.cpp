// Checks that a theme layer with the Add combine is drawn added to what is under it (a HUD
// button's rollover glow, opaque black where it adds nothing), not painted over it: a small
// theme written here, its rollover's glow layer read back with its colours kept and no alpha,
// and the ordinary layer under it unchanged.

#include "render/Theme.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace oc4::render;
namespace fs = std::filesystem;

namespace
{
	int failures = 0;

	void check(bool ok, const char* what)
	{
		if (!ok)
		{
			std::printf("FAILED: %s\n", what);
			++failures;
		}
	}

	// A 2 x 4 top-down 32-bit TGA: rows 0-1 a half-transparent grey, rows 2-3 opaque, dark grey
	// on black (the glow).
	void writeTga(const fs::path& file)
	{
		std::vector<unsigned char> d(18, 0);
		d[2] = 2, d[12] = 2, d[14] = 4, d[16] = 32, d[17] = 0x28;
		const unsigned char rows[4][4] = { { 80, 80, 80, 128 }, { 80, 80, 80, 128 }, { 0x1a, 0x1a, 0x1a, 255 }, { 0, 0, 0, 255 } };
		for (const auto& r : rows)
			for (int x = 0; x < 2; ++x)
				d.insert(d.end(), r, r + 4);
		std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
	}

	// The picture's pixel (BGRA) at (x, y) of the bitmap's part.
	const unsigned char* pixel(const Theme::Bitmap& b, int x, int y)
	{
		return &b.picture->bgra[(size_t(int(b.y0) + y) * b.picture->width + size_t(int(b.x0) + x)) * 4];
	}
}

int main()
{
	const fs::path dir = fs::current_path() / "theme_combine_test";
	fs::remove_all(dir);
	fs::create_directories(dir / "Assets");
	fs::create_directories(dir / "Resource");
	writeTga(dir / "Resource/glow.tga");
	std::ofstream(dir / "Resource/Civ4.thm") << R"(
style SF_CtrlTheme_Civ4_Control_Button_Test_Style
{
	with GFC_CtrlTheme_Bitmap_Control_Button_Skin
	{
		.Default
		{
			layer {
				LayerFlags		= GFlags(GCtrlBitmapSkin, GFC_BITMAPSKIN_CENTER_FIXED);
				DestCombine		= GColorCombine(BlendSource,Disable);
				Image			= GBitmap("glow.tga", HVGrid, 1, 2, 0, 0);
				Margin			= GRectMargin(0,0,0,0);
			}
		}
		.Rollover
		{
			layer { Inherit = .Default; }
			layer {}
			layer {
				LayerFlags		= GFlags(GCtrlBitmapSkin, GFC_BITMAPSKIN_CENTER_FIXED);
				DestCombine		= GColorCombine(Add,Disable);
				Image			= GBitmap("glow.tga", HVGrid, 1, 2, 0, 1);
				Margin			= GRectMargin(0,0,0,0);
			}
		}
	}
}
)";

	Theme theme;
	check(theme.load({ dir / "Assets" }), "theme loads");
	for (const std::string& p : theme.problems())
		std::printf("  theme problem: %s\n", p.c_str());
	const Theme::Button* b = theme.button("Button_Test_Style");
	check(b != nullptr, "button style found");
	if (b)
	{
		const Theme::Skin& normal = b->skin[size_t(Theme::State::Default)];
		const Theme::Skin& over = b->skin[size_t(Theme::State::Rollover)];
		check(normal.size() == 1, "default: one layer");
		check(over.size() == 2, "rollover: the default's layer and the glow");
		if (normal.size() == 1 && normal[0].image)
		{
			const unsigned char* p = pixel(normal[0].image, 0, 0);
			check(p[3] == 128, "ordinary layer keeps its alpha");
		}
		if (over.size() == 2 && over[1].image)
		{
			const unsigned char* glow = pixel(over[1].image, 0, 0);
			const unsigned char* black = pixel(over[1].image, 0, 1);
			check(glow[3] == 0, "added layer has no alpha (adds, does not cover)");
			check(glow[0] == 0x1a && glow[1] == 0x1a && glow[2] == 0x1a, "added layer keeps its colour");
			check(black[0] == 0 && black[1] == 0 && black[2] == 0 && black[3] == 0, "added black is nothing at all");
			check(over[0].image && pixel(over[0].image, 0, 0)[3] == 128, "the inherited layer under it unchanged");
		}
	}
	fs::remove_all(dir);
	if (failures == 0)
		std::printf("theme_combine: all checks passed\n");
	return failures == 0 ? 0 : 1;
}
