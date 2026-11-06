#include "render/Particles.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace oc4::render
{
	namespace
	{
		// A float key group at t: linear between its keys, held before the first and after the last.
		float sample(const nif::KeyGroup<float>& g, float t)
		{
			const auto& k = g.keys;
			if (k.empty())
				return 0;
			if (t <= k.front().time)
				return k.front().value;
			if (t >= k.back().time)
				return k.back().value;
			const auto it = std::upper_bound(k.begin(), k.end(), t, [](float v, const auto& key) { return v < key.time; });
			const auto& b = *it;
			const auto& a = *(it - 1);
			const float s = b.time > a.time ? (t - a.time) / (b.time - a.time) : 0;
			return a.value + (b.value - a.value) * s;
		}

		nif::Color4 sample(const nif::KeyGroup<nif::Color4>& g, float t)
		{
			const auto& k = g.keys;
			if (k.empty())
				return { 1, 1, 1, 1 };
			if (t <= k.front().time)
				return k.front().value;
			if (t >= k.back().time)
				return k.back().value;
			const auto it = std::upper_bound(k.begin(), k.end(), t, [](float v, const auto& key) { return v < key.time; });
			const auto& b = it->value;
			const auto& a = (it - 1)->value;
			const float s = it->time > (it - 1)->time ? (t - (it - 1)->time) / (it->time - (it - 1)->time) : 0;
			return { a.r + (b.r - a.r) * s, a.g + (b.g - a.g) * s, a.b + (b.b - a.b) * s, a.a + (b.a - a.a) * s };
		}

		// On/off keys at t: the last at or before it.
		bool step(const nif::KeyGroup<uint8_t>& g, float t)
		{
			bool on = true;
			for (const auto& k : g.keys)
				if (k.time <= t)
					on = k.value != 0;
				else
					break;
			return on;
		}

		Affine inverse(const Affine& a)
		{
			Affine i;
			i.rotation = transpose(a.rotation);
			i.scale = a.scale != 0 ? 1 / a.scale : 1;
			i.translation = i.rotation * (a.translation * -i.scale);
			return i;
		}

		uint32_t pack(float r, float g, float b, float a)
		{
			const auto c = [](float v) { return uint32_t(std::clamp(v, 0.0f, 1.0f) * 255 + 0.5f); };
			return c(r) | c(g) << 8 | c(b) << 16 | c(a) << 24;
		}
	}

	Particles::Particles(const scene::Model& model, uint32_t seed) : m_state(seed ? seed : 1)
	{
		for (const auto& p : model.particles)
			m_systems.push_back({ &p, {}, 0, 0 });
	}

	float Particles::random()
	{
		// (xorshift: the effects look different from instance to instance, not from run to run)
		m_state ^= m_state << 13;
		m_state ^= m_state >> 17;
		m_state ^= m_state << 5;
		return float(m_state & 0xFFFFFF) / float(0x1000000);
	}

	size_t Particles::alive() const
	{
		size_t n = 0;
		for (const auto& s : m_systems)
			n += s.particles.size();
		return n;
	}

	void Particles::update(const scene::Model& model, float dt, std::span<const Affine> world, std::span<const uint8_t> visible)
	{
		(void)model;
		dt = std::clamp(dt, 0.0f, 0.25f);
		for (System& s : m_systems)
		{
			const scene::ParticleSystem& d = *s.desc;
			const auto nodeWorld = [&](int n) { return n >= 0 && size_t(n) < world.size() ? world[size_t(n)] : Affine{}; };
			const Affine frame = nodeWorld(d.node), toFrame = inverse(frame);
			// Age, pull and move the living.
			for (Particle& p : s.particles)
			{
				p.age += dt;
				for (const auto& g : d.gravity)
				{
					const Affine gw = nodeWorld(g.node >= 0 ? g.node : d.node);
					Vec3 pull;
					if (g.point)
					{
						const Vec3 at = d.worldSpace ? p.position : frame.point(p.position);
						const Vec3 to = gw.translation - at;
						const float len = length(to);
						pull = len > 1e-4f ? to * (1 / len) : Vec3{};
					}
					else
						pull = normalize(gw.vector(g.axis));
					// (in the world: as strong as its node is scaled, like the speeds)
					if (!d.worldSpace)
						pull = toFrame.vector(pull);
					else
						pull = pull * gw.scale;
					p.velocity = p.velocity + pull * (g.strength * dt);
				}
				p.position = p.position + p.velocity * dt;
				p.spin += p.spinSpeed * dt;
			}
			std::erase_if(s.particles, [](const Particle& p) { return p.age >= p.life; });

			// Birth: as many a second as the controller's rate, while it emits and shows.
			s.time += dt;
			float t = d.start + s.time * d.frequency;
			const float span = d.stop - d.start;
			if (span > 0)
				t = d.loop ? d.start + std::fmod(s.time * d.frequency, span) : std::min(t, d.stop);
			const bool shown = d.node < 0 || size_t(d.node) >= visible.size() || visible[size_t(d.node)];
			// (an "on" key passed since the last step: a burst, even when "off" follows at once)
			bool burst = false;
			for (const auto& k : d.activeKeys.keys)
				if (k.value && (s.last < t ? k.time > s.last && k.time <= t : k.time <= t || k.time > s.last))
					burst = true;
			s.last = t;
			const bool emits = !m_stopped && shown && (d.activeKeys.keys.empty() || step(d.activeKeys, t) || burst);
			const float rate = d.birthRateKeys.keys.empty() ? d.birthRate : sample(d.birthRateKeys, t);
			s.toBirth = emits ? std::min(s.toBirth + std::max(rate, 0.0f) * std::max(dt, burst ? 1.0f : 0.0f), float(d.maxParticles) + 1) : 0;
			const Affine emitter = nodeWorld(d.emitterNode >= 0 ? d.emitterNode : d.node);
			while (s.toBirth >= 1)
			{
				s.toBirth -= 1;
				if (s.particles.size() >= d.maxParticles)
					continue;
				Vec3 local;
				switch (d.shape)
				{
				case scene::ParticleSystem::Shape::Box:
					local = { (random() - 0.5f) * d.size.x, (random() - 0.5f) * d.size.y, (random() - 0.5f) * d.size.z };
					break;
				case scene::ParticleSystem::Shape::Sphere:
				{
					const float r = d.size.x * std::cbrt(random()), a = random() * 2 * std::numbers::pi_v<float>, z = random() * 2 - 1, h = std::sqrt(1 - z * z);
					local = { r * h * std::cos(a), r * h * std::sin(a), r * z };
					break;
				}
				case scene::ParticleSystem::Shape::Cylinder:
				{
					const float r = d.size.x * std::sqrt(random()), a = random() * 2 * std::numbers::pi_v<float>;
					local = { r * std::cos(a), r * std::sin(a), (random() - 0.5f) * d.size.y };
					break;
				}
				default:
					break;
				}
				const float decl = d.declination + (random() * 2 - 1) * d.declinationVariation;
				const float planar = d.planarAngle + (random() * 2 - 1) * d.planarAngleVariation;
				const Vec3 dir{ std::sin(decl) * std::cos(planar), std::sin(decl) * std::sin(planar), std::cos(decl) };
				const float speed = d.speed + (random() - 0.5f) * d.speedVariation;
				Particle p;
				p.position = emitter.point(local);
				p.velocity = normalize(emitter.vector(dir)) * (speed * emitter.scale);
				if (!d.worldSpace)
				{
					p.position = toFrame.point(p.position);
					p.velocity = toFrame.vector(p.velocity) * toFrame.scale;
				}
				p.life = std::max(0.05f, d.lifeSpan + (random() - 0.5f) * d.lifeSpanVariation);
				p.radius = std::max(0.0f, d.radius + (random() - 0.5f) * d.radiusVariation) * (d.worldSpace ? emitter.scale : 1);
				if (d.spins)
				{
					p.spinSpeed = d.spinSpeed + (random() * 2 - 1) * d.spinSpeedVariation;
					if (d.spinRandomSign && random() < 0.5f)
						p.spinSpeed = -p.spinSpeed;
					p.spin = d.spinAngle + (random() * 2 - 1) * d.spinAngleVariation;
				}
				s.particles.push_back(p);
			}
		}
	}

	void Particles::quads(size_t system, std::span<const Affine> world, Vec3 right, Vec3 up, std::vector<scene::Vertex>& out) const
	{
		if (system >= m_systems.size())
			return;
		const System& s = m_systems[system];
		const scene::ParticleSystem& d = *s.desc;
		const Affine frame = d.node >= 0 && size_t(d.node) < world.size() ? world[size_t(d.node)] : Affine{};
		const Vec3 normal = normalize(cross(up, right));
		for (const Particle& p : s.particles)
		{
			// (grown in, faded out: its size)
			float grow = 1;
			if (d.growTime > 0 && p.age < d.growTime)
				grow = p.age / d.growTime;
			if (d.fadeTime > 0 && p.life - p.age < d.fadeTime)
				grow = std::min(grow, (p.life - p.age) / d.fadeTime);
			const float r = p.radius * grow * (d.worldSpace ? 1 : frame.scale);
			if (r <= 0)
				continue;
			const Vec3 at = d.worldSpace ? p.position : frame.point(p.position);
			const nif::Color4 c = d.colorKeys.keys.empty() ? d.color : sample(d.colorKeys, p.age / p.life);
			const uint32_t color = pack(c.r, c.g, c.b, c.a);
			const float cs = std::cos(p.spin), sn = std::sin(p.spin);
			const Vec3 rx = (right * cs + up * sn) * r, uy = (up * cs - right * sn) * r;
			const Vec3 corners[4] = { at - rx - uy, at + rx - uy, at + rx + uy, at - rx + uy };
			const Vec2 uvs[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
			for (const int i : { 0, 1, 2, 0, 2, 3 })
			{
				scene::Vertex v;
				v.position = corners[i];
				v.normal = normal;
				v.uv = uvs[i];
				v.color = color;
				v.weights[0] = 255;
				out.push_back(v);
			}
		}
	}
}
