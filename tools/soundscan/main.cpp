// Decodes every sound under a folder (.wav, .mp3) as the engine does and reports those it
// cannot read: soundscan <folder> [--play <file>]
// --play also plays one file through the output device (to hear the decoders).

#include "audio/Audio.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static std::vector<std::byte> readFile(const fs::path& p)
{
	std::ifstream in(p, std::ios::binary);
	std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::vector<std::byte> out(raw.size());
	std::memcpy(out.data(), raw.data(), raw.size());
	return out;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr, "usage: soundscan <folder> [--play <file>]\n");
		return 2;
	}
	if (argc >= 4 && std::string(argv[2]) == "--play")
	{
		std::string error;
		auto pcm = oc4::audio::decode(readFile(argv[3]), &error);
		if (!pcm)
		{
			std::fprintf(stderr, "%s: %s\n", argv[3], error.c_str());
			return 1;
		}
		std::printf("%s: %u Hz, %u channels, %.2f s\n", argv[3], pcm->rate, pcm->channels, pcm->seconds());
		oc4::audio::Device device;
		const auto sound = std::make_shared<const oc4::audio::Pcm>(std::move(*pcm));
		const auto voice = device.play(sound, 1.0f);
		while (device.playing(voice))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			device.update();
		}
		return 0;
	}
	size_t ok = 0, failed = 0;
	double seconds = 0;
	std::map<std::string, size_t> errors;
	const auto start = std::chrono::steady_clock::now();
	for (const auto& e : fs::recursive_directory_iterator(argv[1], fs::directory_options::skip_permission_denied))
	{
		if (!e.is_regular_file())
			continue;
		std::string ext = e.path().extension().string();
		for (char& c : ext)
			c = char(std::tolower(static_cast<unsigned char>(c)));
		if (ext != ".wav" && ext != ".mp3")
			continue;
		std::string error;
		if (const auto pcm = oc4::audio::decode(readFile(e.path()), &error); pcm && !pcm->samples.empty())
			++ok, seconds += pcm->seconds();
		else
		{
			++failed;
			if (errors[error.empty() ? "empty" : error]++ < 3)
				std::printf("  %s: %s\n", e.path().string().c_str(), error.empty() ? "no samples" : error.c_str());
		}
	}
	std::printf("%zu decoded (%.0f minutes of sound), %zu failed, in %.1f s\n", ok, seconds / 60, failed,
		std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
	for (const auto& [why, n] : errors)
		std::printf("  %zu: %s\n", n, why.c_str());
	return failed ? 1 : 0;
}
