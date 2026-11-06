// nifview: browse, animate and render Civ4 NIF models from the installed game.
//
//   nifview [--civ4 <root>] [--filter <text>] [--size WxH] [--yaw deg] [--pitch deg]
//           [--anim <clip>] [--to <clip> --at <seconds>] [--damage 0-4] [--time <seconds>]
//           [--bind] [--screenshot out.png] [--list] [model key or file]
//
// <clip> is an event code (1000) or part of a clip name (strikea). --civ4 defaults to
// %OC4_CIV4_DIR%. --filter keeps models whose asset key contains the text (e.g.
// "art/units/"). With --screenshot the animation is simulated for --time seconds
// (switching to --to at --at seconds through the KFM transition), rendered offscreen,
// saved as PNG and the program exits.
//
// Animations come from the .kfm next to the model (falling back to every .kf in the
// model's folder). Non-looping clips return to idle like units do in game.
//
// Keys: Left/Right previous/next model, PgUp/PgDn +-10, Home/End, Up/Down previous/next
// animation, T smooth/instant switching, D damage level, Space pause, +/- speed,
// B bind pose, F frame, S save screenshot, Esc quit.
// Mouse: left drag orbit, right drag pan, wheel zoom.

#include "render/PngWriter.h"
#include "render/Particles.h"
#include "render/Input.h"
#include "render/Renderer.h"

#include "anim/Animation.h"
#include "core/AssetResolver.h"
#include "core/FileUtil.h"
#include "nif/KfmFile.h"
#include "nif/NifFile.h"
#include "scene/NifScene.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <windows.h>
#include <objbase.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace oc4;
using namespace oc4::render;

namespace
{
	std::string narrow(std::wstring_view w)
	{
		if (w.empty())
			return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
		std::string s(size_t(n), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
		return s;
	}

	std::string folderOf(const std::string& key) { return key.substr(0, key.rfind('/') + 1); }
	std::string stemOf(const std::string& key)
	{
		const std::string name = key.substr(key.rfind('/') + 1);
		return name.substr(0, name.rfind('.'));
	}

	constexpr int kMaxDamage = 4;

	struct Options
	{
		fs::path civ4Root;
		std::optional<fs::path> modAssets; // --mod <mod folder>: its Assets over the game's
		std::string filter;
		std::string model;
		std::optional<fs::path> screenshot;
		uint32_t width = 1280, height = 800;
		std::optional<float> yawDeg, pitchDeg;
		std::string anim, to;
		float at = 0, time = 0;
		int damage = 0;
		bool bindPose = false;
		bool list = false;
	};

	class App
	{
	public:
		App(const Options& opt, SDL_Window* window)
			: m_opt(opt), m_assets(AssetResolver::forInstall(opt.civ4Root, opt.modAssets)), m_renderer(window, opt.width, opt.height), m_window(window)
		{
			if (m_assets.layerCount() == 0)
				throw std::runtime_error("no Assets folders under " + opt.civ4Root.string());
			for (auto& loc : m_assets.list(".nif"))
				if (opt.filter.empty() || loc.key.find(opt.filter) != std::string::npos)
					m_models.push_back(std::move(loc));
			for (auto& loc : m_assets.list(".kfm"))
				m_kfmByFolder.emplace(folderOf(loc.key), std::move(loc));
			for (auto& loc : m_assets.list(".kf"))
				m_kfByFolder.emplace(folderOf(loc.key), std::move(loc));
			m_bindPose = opt.bindPose;

			if (!opt.model.empty())
			{
				std::optional<AssetLocation> wanted = fs::is_regular_file(opt.model) ? m_assets.locateFile(opt.model) : m_assets.locate(opt.model);
				if (!wanted)
					throw std::runtime_error("model not found: " + opt.model);
				const auto it = std::ranges::find(m_models, wanted->key, &AssetLocation::key);
				if (it == m_models.end())
				{
					m_models.insert(m_models.begin(), *wanted);
					m_index = 0;
				}
				else
					m_index = size_t(it - m_models.begin());
			}
			if (m_models.empty())
				throw std::runtime_error("no models match the filter");
			load(m_index);
		}

		// Plays the command-line scenario for a screenshot: --anim, then --to at --at, for --time seconds.
		void simulate()
		{
			if (!m_opt.anim.empty())
				if (const auto c = findClip(m_opt.anim))
					m_player.play(*c, false);
			setDamage(m_opt.damage);
			const auto target = m_opt.to.empty() ? std::nullopt : findClip(m_opt.to);
			constexpr float kStep = 1.0f / 120.0f;
			bool switched = false;
			for (float t = 0; t + kStep * 0.5f < m_opt.time; t += kStep)
			{
				if (target && !switched && t >= m_opt.at)
				{
					m_player.play(*target, true);
					switched = true;
				}
				m_player.update(kStep);
				stepParticles(kStep);
			}
			if (m_particles.any())
				std::cout << std::format("  {} particles alive\n", m_particles.alive());
		}

		// The model's particles kStep or dt on, where its nodes are now.
		void stepParticles(float dt)
		{
			if (!m_particles.any())
				return;
			const anim::Pose pose = m_bindPose ? anim::Pose::bind(m_model) : m_player.evaluate(m_model);
			const auto world = m_model.worldTransforms(pose.locals);
			m_particles.update(m_model, dt, world, m_model.visibility(pose.hidden));
		}

		void load(size_t index)
		{
			m_index = std::min(index, m_models.size() - 1);
			const AssetLocation& loc = m_models[m_index];
			m_status.clear();
			m_player = {};
			m_damage = 0;
			try
			{
				const auto shared = std::make_shared<const nif::File>(nif::parseFile(m_assets.read(loc)));
				const nif::File& file = *shared;
				m_model = scene::buildModel(file);
				m_gpu = m_renderer.upload(m_model, m_assets, loc);
				m_particles = render::Particles(m_model);
				const auto& s = m_gpu->stats();
				const Vec3 size = m_model.bounds.empty() ? Vec3{} : m_model.bounds.max - m_model.bounds.min;
				m_status = std::format("{} items, {} verts, {} tris, {} tex, {:.0f} x {:.0f} x {:.0f}{}{}{}", s.drawItems, s.vertices, s.triangles, s.textures,
					size.x, size.y, size.z,
					s.missingTextures ? std::format(" ({} missing)", s.missingTextures) : "",
					m_model.particleSystems ? std::format(", {} particle systems ({} run)", m_model.particleSystems, m_model.particles.size()) : "",
					file.complete() ? "" : " [PARTIAL]");
				loadClips(loc, shared);
			}
			catch (const std::exception& e)
			{
				m_model = {};
				m_gpu = m_renderer.upload(m_model, m_assets, loc);
				m_status = std::string("error: ") + e.what();
			}
			frame();
			std::cout << std::format("[{}/{}] {}  -  {}, {} animations\n", m_index + 1, m_models.size(), loc.key, m_status, m_player.clips().size());
			// (NV_ITEMS=1: each draw item's node and how it is drawn)
			if (std::getenv("NV_ITEMS"))
				for (const auto& it : m_model.items)
					std::cout << std::format("  item {} [{}]: blend {} test {} ztest {} zwrite {} skinned {}\n", it.nodeName, it.material.shaderName, it.material.alphaBlend, it.material.alphaTest,
						it.material.zTest, it.material.zWrite, it.skinned);
			for (const auto& w : m_model.warnings)
				std::cout << "  warning: " << w << '\n';
			// (the particle systems: their emitters and rates)
			for (const auto& p : m_model.particles)
				std::cout << std::format("  particles {}: shape {} size {:.1f},{:.1f},{:.1f} rate {:.1f} ({} keys, {} on/off: {}) life {:.2f} radius {:.1f} speed {:.1f} max {} time {:.2f}..{:.2f}{} grow {:.2f} fade {:.2f} ws {} colors {} grav {}\n",
					p.nodeName, int(p.shape), p.size.x, p.size.y, p.size.z, p.birthRateKeys.keys.empty() ? p.birthRate : p.birthRateKeys.keys[0].value, p.birthRateKeys.keys.size(), p.activeKeys.keys.size(), [&]{ std::string t; for (auto& k : p.activeKeys.keys) t += std::format("{:.2f}={} ", k.time, int(k.value)); return t; }(),  p.lifeSpan, p.radius, p.speed,
					p.maxParticles, p.start, p.stop, p.loop ? " loop" : "", p.growTime, p.fadeTime, p.worldSpace, p.colorKeys.keys.size(), p.gravity.size());
			for (const auto& t : m_gpu->stats().missing)
				std::cout << "  missing texture: " << t << '\n';
			for (const auto& c : m_player.clips())
				std::cout << std::format("  anim {:<44} {:6.2f}s {}  {} xform, {} vis, {} material{}\n", c.label, c.sequence.duration(),
					c.sequence.loops() ? "loop " : "once ", c.sequence.transformChannels(), c.sequence.visibilityChannels(),
					c.sequence.materialChannels(), c.sequence.unboundChannels() ? std::format(", {} unbound", c.sequence.unboundChannels()) : "");
			updateTitle();
		}

		std::shared_ptr<const nif::File> loadKf(const std::string& key)
		{
			if (const auto it = m_kfCache.find(key); it != m_kfCache.end())
				return it->second;
			std::shared_ptr<const nif::File> file;
			if (const auto kf = m_assets.locate(key))
				file = std::make_shared<nif::File>(nif::parseFile(m_assets.read(*kf)));
			m_kfCache.emplace(key, file);
			return file;
		}

		void loadClips(const AssetLocation& model, const std::shared_ptr<const nif::File>& nif)
		{
			const std::string folder = folderOf(model.key);
			const std::string modelFile = model.key.substr(folder.size());

			// Prefer a KFM that names this model; otherwise any KFM in the folder.
			std::optional<nif::KfmFile> kfm;
			for (const bool exact : { true, false })
			{
				for (auto [it, end] = m_kfmByFolder.equal_range(folder); it != end && !kfm; ++it)
					try
					{
						nif::KfmFile f = nif::parseKfm(m_assets.read(it->second));
						if (!exact || makeAssetKey(f.nifFile) == modelFile)
							kfm = std::move(f);
					}
					catch (const std::exception&)
					{
					}
				if (kfm)
					break;
			}

			std::vector<anim::Clip> clips;
			const auto add = [&](std::string label, uint32_t code, const std::string& kfKey, uint32_t index) {
				if (const auto kf = loadKf(kfKey))
					if (anim::Sequence seq = anim::Sequence::bind(kf, index, m_model); seq.channelCount() > 0)
						clips.push_back({ std::move(label), code, std::move(seq) });
			};
			if (kfm)
				for (const auto& a : kfm->animations)
					add(std::format("[{}] {}", a.eventCode, stemOf(makeAssetKey(a.kfFile))), a.eventCode, makeAssetKey(folder + a.kfFile), a.sequenceIndex);
			else
				for (auto [it, end] = m_kfByFolder.equal_range(folder); it != end; ++it)
					add(stemOf(it->second.key), 0, it->second.key, 0);

			// Without animations of its own, what its controllers do (flowing water, turning wheels).
			if (clips.empty())
				if (anim::Sequence own = anim::Sequence::embedded(nif, m_model); own.channelCount() > 0)
					clips.push_back({ "embedded controllers", 1000, std::move(own) });
			m_player = anim::Player(std::move(clips), std::move(kfm));
			const auto idle = m_player.find(1000);
			m_player.setFallback(idle);
			if (idle)
				m_player.play(*idle, false);
		}

		std::optional<size_t> findClip(const std::string& what) const
		{
			const auto& clips = m_player.clips();
			if (!what.empty() && std::isdigit(static_cast<unsigned char>(what[0])))
				if (const auto c = m_player.find(uint32_t(std::stoul(what))))
					return c;
			for (size_t i = 0; i < clips.size(); ++i)
				if (toLowerAscii(clips[i].label).find(toLowerAscii(what)) != std::string::npos)
					return i;
			std::cout << "  no animation matches '" << what << "'\n";
			return std::nullopt;
		}

		// Civ4 damage states: event code stage*1000 + 500 + part*10 per damaged part.
		void setDamage(int level)
		{
			m_damage = std::clamp(level, 0, kMaxDamage);
			std::vector<size_t> layers;
			if (m_damage > 0)
				for (uint32_t part = 0; part < 10; ++part)
					if (const auto c = m_player.find(uint32_t(m_damage) * 1000 + 500 + part * 10))
						layers.push_back(*c);
			m_player.setLayers(std::move(layers));
		}

		void updateTitle()
		{
			std::string anim = m_bindPose ? "bind pose" : m_player.describe();
			if (m_damage)
				anim += std::format(", damage {} ({} layers)", m_damage, m_player.layers().size());
			if (m_paused)
				anim += ", paused";
			if (m_speed != 1.0f)
				anim += std::format(", x{:.2f}", m_speed);
			if (!m_smooth)
				anim += ", instant switching";
			m_title = std::format("[{}/{}] {} - {} - {}", m_index + 1, m_models.size(), m_models[m_index].key, anim, m_status);
			if (m_window)
				SDL_SetWindowTitle(m_window, ("nifview " + m_title).c_str());
		}

		void renderPose()
		{
			const anim::Pose pose = m_bindPose ? anim::Pose::bind(m_model) : m_player.evaluate(m_model);
			std::vector<scene::Material> materials;
			materials.reserve(m_model.items.size());
			for (const auto& item : m_model.items)
				materials.push_back(anim::animatedMaterial(item, pose));
			m_renderer.beginFrame(m_camera);
			const auto world = m_model.worldTransforms(pose.locals);
			m_renderer.draw(*m_gpu, world, m_model.visibility(pose.hidden), materials);
			// (the particles facing the camera)
			const Vec3 forward = normalize(m_camera.target - m_camera.eye()), right = normalize(cross(forward, Vec3{ 0, 0, 1 })), up = cross(right, forward);
			for (size_t i = 0; i < m_particles.systems(); ++i)
			{
				std::vector<scene::Vertex> quads;
				m_particles.quads(i, world, right, up, quads);
				m_renderer.drawParticles(*m_gpu, i, m_model.particles[i].material, std::move(quads));
			}
			m_renderer.endFrame();
		}

		void frame()
		{
			m_camera = {};
			if (m_opt.yawDeg)
				m_camera.yaw = *m_opt.yawDeg * 3.14159265f / 180.0f;
			if (m_opt.pitchDeg)
				m_camera.pitch = *m_opt.pitchDeg * 3.14159265f / 180.0f;
			if (m_model.bounds.empty())
			{
				// (particles alone, an effect: from a distance, a little above the ground)
				if (!m_model.particles.empty())
					m_camera.target = { 0, 0, 60 }, m_camera.distance = 500;
				return;
			}
			m_camera.target = m_model.bounds.center();
			const float radius = std::max(m_model.bounds.radius(), 0.01f);
			m_camera.distance = radius / std::sin(m_camera.fovY * 0.5f) * 1.05f;
		}

		void tick(float dt)
		{
			if (!m_paused)
			{
				const bool wasTransition = m_player.inTransition();
				const size_t wasClip = m_player.current();
				m_player.update(dt * m_speed);
				stepParticles(dt * m_speed);
				if (wasTransition != m_player.inTransition() || wasClip != m_player.current() || m_player.inTransition())
					updateTitle();
			}
			renderPose();
			m_renderer.present();
		}

		void screenshot(const fs::path& path)
		{
			renderPose();
			uint32_t w = 0, h = 0;
			const auto pixels = m_renderer.readback(w, h);
			writePng(path, pixels, w, h);
			std::cout << std::format("saved {}  ({} at {:.2f}s{})\n", path.string(), m_bindPose ? "bind pose" : m_player.describe(), m_player.time(),
				m_damage ? std::format(", damage {}", m_damage) : "");
		}

		void step(ptrdiff_t delta)
		{
			const ptrdiff_t n = ptrdiff_t(m_models.size());
			load(size_t(((ptrdiff_t(m_index) + delta) % n + n) % n));
		}

		void stepClip(ptrdiff_t delta)
		{
			const auto& clips = m_player.clips();
			if (clips.empty())
				return;
			const ptrdiff_t n = ptrdiff_t(clips.size());
			m_player.play(size_t(((ptrdiff_t(m_player.current()) + delta) % n + n) % n), m_smooth);
			updateTitle();
		}

		void onKey(unsigned key)
		{
			switch (key)
			{
			case render::key::Right: case 'N': step(1); break;
			case render::key::Left: case 'P': step(-1); break;
			case render::key::Next: step(10); break;
			case render::key::Prior: step(-10); break;
			case render::key::Home: load(0); break;
			case render::key::End: load(m_models.size() - 1); break;
			case render::key::Down: stepClip(1); break;
			case render::key::Up: stepClip(-1); break;
			case 'T': m_smooth = !m_smooth; updateTitle(); break;
			case 'D': setDamage((m_damage + 1) % (kMaxDamage + 1)); updateTitle(); break;
			case render::key::Space: m_paused = !m_paused; updateTitle(); break;
			case render::key::Plus: m_speed = std::min(m_speed * 1.5f, 8.0f); updateTitle(); break;
			case render::key::Minus: m_speed = std::max(m_speed / 1.5f, 0.05f); updateTitle(); break;
			case 'B': m_bindPose = !m_bindPose; updateTitle(); break;
			case 'F': frame(); break;
			case 'S':
				screenshot(fs::current_path() / (stemOf(m_models[m_index].key) + ".png"));
				break;
			case render::key::Escape: quit = true; break;
			}
		}

		void onMouseMove(int x, int y, SDL_MouseButtonFlags buttons)
		{
			const int dx = x - m_lastX, dy = y - m_lastY;
			m_lastX = x;
			m_lastY = y;
			if (buttons & SDL_BUTTON_LMASK)
			{
				m_camera.yaw -= dx * 0.01f;
				m_camera.pitch = std::clamp(m_camera.pitch + dy * 0.01f, -1.55f, 1.55f);
			}
			else if (buttons & SDL_BUTTON_RMASK)
			{
				const Vec3 forward = normalize(m_camera.target - m_camera.eye());
				const Vec3 right = normalize(cross(forward, Vec3{ 0, 0, 1 }));
				const Vec3 up = cross(right, forward);
				const float scale = m_camera.distance * 0.0015f;
				m_camera.target = m_camera.target - right * (dx * scale) + up * (dy * scale);
			}
		}

		void onWheel(int delta) { m_camera.distance *= std::pow(0.9f, delta / 120.0f); }
		void onResize(uint32_t w, uint32_t h) { m_renderer.resize(w, h); }

		bool quit = false;

	private:
		Options m_opt;
		AssetResolver m_assets;
		Renderer m_renderer;
		std::unique_ptr<GpuModel> m_gpu;
		render::Particles m_particles;
		SDL_Window* m_window = nullptr;
		std::vector<AssetLocation> m_models;
		std::multimap<std::string, AssetLocation> m_kfmByFolder, m_kfByFolder;
		std::map<std::string, std::shared_ptr<const nif::File>> m_kfCache;
		size_t m_index = 0;
		scene::Model m_model;
		anim::Player m_player;
		int m_damage = 0;
		float m_speed = 1;
		bool m_paused = false, m_bindPose = false, m_smooth = true;
		Camera m_camera;
		std::string m_title, m_status;
		int m_lastX = 0, m_lastY = 0;
	};

	Options parseArgs(int argc, wchar_t** argv)
	{
		Options o;
		if (wchar_t env[MAX_PATH]; GetEnvironmentVariableW(L"OC4_CIV4_DIR", env, MAX_PATH) > 0)
			o.civ4Root = env;
		for (int i = 1; i < argc; ++i)
		{
			const std::wstring a = argv[i];
			const auto next = [&]() -> std::wstring {
				if (i + 1 >= argc)
					throw std::runtime_error("missing value for " + narrow(a));
				return argv[++i];
			};
			if (a == L"--civ4")
				o.civ4Root = next();
			else if (a == L"--mod")
				o.modAssets = fs::path(next()) / "Assets";
			else if (a == L"--filter")
				o.filter = narrow(next());
			else if (a == L"--screenshot")
				o.screenshot = fs::path(next());
			else if (a == L"--yaw")
				o.yawDeg = std::stof(next());
			else if (a == L"--pitch")
				o.pitchDeg = std::stof(next());
			else if (a == L"--anim")
				o.anim = narrow(next());
			else if (a == L"--to")
				o.to = narrow(next());
			else if (a == L"--at")
				o.at = std::stof(next());
			else if (a == L"--time")
				o.time = std::stof(next());
			else if (a == L"--damage")
				o.damage = std::stoi(next());
			else if (a == L"--bind")
				o.bindPose = true;
			else if (a == L"--list")
				o.list = true;
			else if (a == L"--size")
			{
				const std::wstring v = next();
				const size_t x = v.find(L'x');
				o.width = std::stoul(v.substr(0, x));
				o.height = std::stoul(v.substr(x + 1));
			}
			else
				o.model = narrow(a);
		}
		if (o.civ4Root.empty())
			throw std::runtime_error("pass --civ4 <folder with Assets, Warlords, Beyond the Sword> or set OC4_CIV4_DIR");
		return o;
	}
}

int wmain(int argc, wchar_t** argv)
{
	try
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		const Options opt = parseArgs(argc, argv);

		if (opt.list)
		{
			for (const auto& loc : AssetResolver::forInstall(opt.civ4Root).list(".nif"))
				if (opt.filter.empty() || loc.key.find(opt.filter) != std::string::npos)
					std::cout << loc.key << '\n';
			return 0;
		}

		if (opt.screenshot)
		{
			App app(opt, nullptr);
			app.simulate();
			app.screenshot(*opt.screenshot);
			return 0;
		}

		if (!SDL_Init(SDL_INIT_VIDEO))
			throw std::runtime_error(std::string("SDL: ") + SDL_GetError());
		SDL_Window* window = SDL_CreateWindow("nifview", int(opt.width), int(opt.height), SDL_WINDOW_RESIZABLE);
		if (!window)
			throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
		{
			App app(opt, window);
			using Clock = std::chrono::steady_clock;
			auto last = Clock::now();
			while (!app.quit)
			{
				for (SDL_Event e; SDL_PollEvent(&e);)
					switch (e.type)
					{
					case SDL_EVENT_QUIT: app.quit = true; break;
					case SDL_EVENT_KEY_DOWN: app.onKey(render::keyOfSdl(e.key.key)); break;
					case SDL_EVENT_MOUSE_MOTION: app.onMouseMove(int(e.motion.x), int(e.motion.y), e.motion.state); break;
					case SDL_EVENT_MOUSE_BUTTON_DOWN: app.onMouseMove(int(e.button.x), int(e.button.y), 0); break;
					case SDL_EVENT_MOUSE_WHEEL: app.onWheel(int(e.wheel.y * 120)); break;
					case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: app.onResize(uint32_t(e.window.data1), uint32_t(e.window.data2)); break;
					}
				const auto now = Clock::now();
				const float dt = std::min(std::chrono::duration<float>(now - last).count(), 0.1f);
				last = now;
				app.tick(dt);
			}
		}
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "nifview: " << e.what() << '\n';
		return 1;
	}
}
