#pragma once

// The game's movies (Bink: the wonders', the victories', the intros), decoded with FFmpeg on a
// thread of their own: the sound all at once, the pictures a few frames ahead of the one shown.

#include "audio/Audio.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace oc4::media
{
	// Whether movies can be played at all (FFmpeg's libraries found beside the program).
	bool available();

	class Movie
	{
	public:
		explicit Movie(const std::filesystem::path& file);
		~Movie();
		Movie(const Movie&) = delete;
		Movie& operator=(const Movie&) = delete;

		struct Frame
		{
			std::vector<uint8_t> bgra;
			uint32_t width = 0, height = 0;
			uint64_t number = 0; // (from 1)
		};

		// It could not be read (no file, no FFmpeg, nothing it can decode).
		bool failed() const;
		// Its sound is decoded and its first picture is ready: it can start.
		bool ready() const;
		// Its sound (null: it has none).
		std::shared_ptr<const audio::Pcm> sound() const;
		// The picture to show this long after the start (null before the first).
		const Frame* frameAt(double seconds);
		// All its pictures shown.
		bool finished() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m;
	};
}
