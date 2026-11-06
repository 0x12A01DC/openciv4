#pragma once

#include <array>
#include <cstdint>

namespace oc4::nif
{
	constexpr uint32_t makeVersion(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
	{
		return (a << 24) | (b << 16) | (c << 8) | d;
	}

	// Civ4 ships almost everything as 20.0.0.4; a handful of old files are 10.x / 4.2.2.0.
	constexpr uint32_t kVersionCiv4 = makeVersion(20, 0, 0, 4);

	using Ref = int32_t; // block index, -1 = none
	constexpr Ref kNoRef = -1;

	struct Vec2
	{
		float x = 0, y = 0;
	};

	struct Vec3
	{
		float x = 0, y = 0, z = 0;
	};

	struct Color3
	{
		float r = 0, g = 0, b = 0;
	};

	struct Color4
	{
		float r = 0, g = 0, b = 0, a = 0;
	};

	struct Quat
	{
		float w = 1, x = 0, y = 0, z = 0;
	};

	// Stored as written in the file: m[row][col].
	struct Mat33
	{
		std::array<std::array<float, 3>, 3> m{ { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
	};

	struct Transform
	{
		Mat33 rotation;
		Vec3 translation;
		float scale = 1;
	};

	using Triangle = std::array<uint16_t, 3>;
}
