#pragma once

#include "nif/NifTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace oc4
{
	using nif::Mat33;
	using nif::Vec2;
	using nif::Vec3;

	inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline Vec3 operator*(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	inline Vec3& operator+=(Vec3& a, Vec3 b) { return a = a + b; }
	inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline Vec3 cross(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
	inline Vec3 normalize(Vec3 a)
	{
		const float l = length(a);
		return l > 1e-20f ? a * (1.0f / l) : Vec3{ 0, 0, 1 };
	}

	// Gamebryo NiMatrix3 convention: m[row][col], column vectors (p' = M p).
	inline Vec3 operator*(const Mat33& m, Vec3 v)
	{
		const auto& e = m.m;
		return { e[0][0] * v.x + e[0][1] * v.y + e[0][2] * v.z,
			e[1][0] * v.x + e[1][1] * v.y + e[1][2] * v.z,
			e[2][0] * v.x + e[2][1] * v.y + e[2][2] * v.z };
	}

	inline Mat33 operator*(const Mat33& a, const Mat33& b)
	{
		Mat33 r;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
		return r;
	}

	inline Mat33 transpose(const Mat33& a)
	{
		Mat33 r;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				r.m[i][j] = a.m[j][i];
		return r;
	}

	using nif::Quat;

	inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

	inline float dot(const Quat& a, const Quat& b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }

	inline Quat normalize(const Quat& q)
	{
		const float l = std::sqrt(dot(q, q));
		if (l < 1e-20f)
			return {};
		const float s = 1.0f / l;
		return { q.w * s, q.x * s, q.y * s, q.z * s };
	}

	inline Quat operator*(const Quat& a, const Quat& b)
	{
		return { a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
			a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
			a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w };
	}

	inline Quat axisAngle(Vec3 axis, float angle)
	{
		const float s = std::sin(angle * 0.5f);
		return { std::cos(angle * 0.5f), axis.x * s, axis.y * s, axis.z * s };
	}

	// Shortest-arc spherical interpolation.
	inline Quat slerp(const Quat& a, Quat b, float t)
	{
		float c = dot(a, b);
		if (c < 0)
		{
			b = { -b.w, -b.x, -b.y, -b.z };
			c = -c;
		}
		float wa = 1 - t, wb = t;
		if (c < 0.9995f)
		{
			const float theta = std::acos(c);
			const float s = 1.0f / std::sin(theta);
			wa = std::sin((1 - t) * theta) * s;
			wb = std::sin(t * theta) * s;
		}
		return normalize(Quat{ a.w * wa + b.w * wb, a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb });
	}

	// Rotation matrix for p' = M p (same convention as NIF matrices).
	inline Mat33 toMat33(const Quat& q)
	{
		const float w = q.w, x = q.x, y = q.y, z = q.z;
		Mat33 m;
		m.m[0] = { 1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y) };
		m.m[1] = { 2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x) };
		m.m[2] = { 2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y) };
		return m;
	}

	// Inverse of toMat33 for pure rotation matrices.
	inline Quat toQuat(const Mat33& r)
	{
		const auto& m = r.m;
		const float trace = m[0][0] + m[1][1] + m[2][2];
		Quat q;
		if (trace > 0)
		{
			const float s = std::sqrt(trace + 1.0f) * 2;
			q = { 0.25f * s, (m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s };
		}
		else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
		{
			const float s = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2;
			q = { (m[2][1] - m[1][2]) / s, 0.25f * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s };
		}
		else if (m[1][1] > m[2][2])
		{
			const float s = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2;
			q = { (m[0][2] - m[2][0]) / s, (m[0][1] + m[1][0]) / s, 0.25f * s, (m[1][2] + m[2][1]) / s };
		}
		else
		{
			const float s = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2;
			q = { (m[1][0] - m[0][1]) / s, (m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25f * s };
		}
		return normalize(q);
	}

	// Rotation + uniform scale + translation, as Gamebryo's NiTransform.
	struct Affine
	{
		Mat33 rotation;
		Vec3 translation;
		float scale = 1;

		static Affine from(const nif::Transform& t) { return { t.rotation, t.translation, t.scale }; }

		Vec3 point(Vec3 p) const { return rotation * (p * scale) + translation; }
		Vec3 vector(Vec3 v) const { return rotation * v; }

		// (a * b)(p) == a(b(p))
		friend Affine operator*(const Affine& a, const Affine& b)
		{
			return { a.rotation * b.rotation, a.point(b.translation), a.scale * b.scale };
		}
	};

	// Component-wise blend: lerp translation/scale, slerp rotation. w=0 -> a, w=1 -> b.
	inline Affine blend(const Affine& a, const Affine& b, float w)
	{
		return { toMat33(slerp(toQuat(a.rotation), toQuat(b.rotation), w)), lerp(a.translation, b.translation, w), a.scale + (b.scale - a.scale) * w };
	}

	struct Bounds
	{
		Vec3 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		Vec3 max{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

		bool empty() const { return min.x > max.x; }
		void add(Vec3 p)
		{
			min = { std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z) };
			max = { std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z) };
		}
		Vec3 center() const { return (min + max) * 0.5f; }
		float radius() const { return length(max - min) * 0.5f; }
	};
}
