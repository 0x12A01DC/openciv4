#include "audio/Audio.h"

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

namespace oc4::audio
{
	std::optional<Pcm> decodeMedia(std::span<const std::byte> file, std::string* error)
	{
		const auto fail = [&](const char* why) -> std::optional<Pcm> {
			if (error)
				*error = why;
			return std::nullopt;
		};
		drmp3_config config{};
		drmp3_uint64 frames = 0;
		drmp3_int16* pcm = drmp3_open_memory_and_read_pcm_frames_s16(file.data(), file.size(), &config, &frames, nullptr);
		if (!pcm)
			return fail("not an MP3");
		Pcm out;
		out.channels = uint16_t(config.channels);
		out.rate = config.sampleRate;
		if (out.channels >= 1 && out.channels <= 2 && out.rate)
			out.samples.assign(pcm, pcm + frames * config.channels);
		drmp3_free(pcm, nullptr);
		if (out.samples.empty())
			return fail("not mono or stereo");
		return out;
	}
}
