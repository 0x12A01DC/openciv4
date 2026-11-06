#pragma once

#include "core/BinaryReader.h"
#include "nif/NifTypes.h"

#include <string>
#include <vector>

namespace oc4::nif
{
	// BinaryReader plus NIF version context and NIF primitive types.
	class NifReader : public BinaryReader
	{
	public:
		NifReader(std::span<const std::byte> data) : BinaryReader(data) {}

		uint32_t version = 0;
		uint32_t userVersion = 0;

		bool since(uint32_t v) const { return version >= v; }
		bool until(uint32_t v) const { return version <= v; }

		// NIF bool is 1 byte from 4.1.0.1, 4 bytes before.
		bool boolean() { return version >= makeVersion(4, 1, 0, 1) ? u8() != 0 : u32() != 0; }
		Ref ref() { return i32(); }

		// Before 20.1.0.1 there is no string table: strings are inline.
		std::string string()
		{
			if (version >= makeVersion(20, 1, 0, 1))
				fail("string table versions not supported");
			return readSizedString();
		}

		Vec2 vec2() { return read<Vec2>(); }
		Vec3 vec3() { return read<Vec3>(); }
		Color3 color3() { return read<Color3>(); }
		Color4 color4() { return read<Color4>(); }
		Quat quat() { return read<Quat>(); }
		Mat33 mat33() { return read<Mat33>(); }

		Transform transform() // NiTransform: rotation, translation, scale
		{
			Transform t;
			t.rotation = mat33();
			t.translation = vec3();
			t.scale = f32();
			return t;
		}

		std::vector<Ref> refList()
		{
			const uint32_t n = u32();
			return readVector<Ref>(n);
		}

		template <class T>
		std::vector<T> vectorOf(size_t n) { return readVector<T>(n); }
	};
}
