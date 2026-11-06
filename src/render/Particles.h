#pragma once

// Particle systems at work (scene::ParticleSystem, Gamebryo's NiParticleSystem as Civ4's art
// has them): a model instance's particles born by its emitters' rates, moved by their speed and
// the pulls on them, grown, faded, coloured and spun over their lives, and turned into quads
// facing the camera for the renderer. One Particles per instance of a model that has any.

#include "core/Math.h"
#include "scene/NifScene.h"

#include <cstdint>
#include <span>
#include <vector>

namespace oc4::render
{
	class Particles
	{
	public:
		Particles() = default;
		explicit Particles(const scene::Model& model, uint32_t seed = 1);

		bool any() const { return !m_systems.empty(); }
		// dt seconds on: the model's nodes in the world now (placed), and which of them show
		// (a hidden system emits nothing, its particles live on).
		void update(const scene::Model& model, float dt, std::span<const Affine> world, std::span<const uint8_t> visible);
		// A system's live particles as quads facing the camera (right and up: the view's unit
		// vectors), six vertices each, in the world; its colour in the vertices' colour.
		void quads(size_t system, std::span<const Affine> world, Vec3 right, Vec3 up, std::vector<scene::Vertex>& out) const;
		size_t systems() const { return m_systems.size(); }
		// (no more born: an effect whose time is up lets its particles die out)
		void stopEmitting() { m_stopped = true; }
		size_t alive() const;

	private:
		struct Particle
		{
			Vec3 position, velocity; // in the world, or in the system's frame (not worldSpace)
			float age = 0, life = 1, radius = 1, spin = 0, spinSpeed = 0;
		};
		struct System
		{
			const scene::ParticleSystem* desc = nullptr;
			std::vector<Particle> particles;
			float time = 0, toBirth = 0, last = -1e30f; // (last: the controller's time a step ago)
		};
		float random();

		std::vector<System> m_systems;
		uint32_t m_state = 1;
		bool m_stopped = false;
	};
}
