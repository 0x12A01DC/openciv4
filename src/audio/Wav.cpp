#include "audio/Audio.h"

#include <algorithm>
#include <cstring>

namespace oc4::audio
{
	namespace
	{
		struct Reader
		{
			std::span<const std::byte> data;
			size_t pos = 0;

			bool has(size_t n) const { return pos + n <= data.size(); }
			uint8_t u8(size_t at) const { return uint8_t(data[at]); }
			uint16_t u16(size_t at) const { return uint16_t(u8(at) | (u8(at + 1) << 8)); }
			uint32_t u32(size_t at) const { return uint32_t(u16(at)) | (uint32_t(u16(at + 2)) << 16); }
		};

		int16_t clamp16(int v) { return int16_t(std::clamp(v, -32768, 32767)); }

		// ---------------------------------------------------------------- IMA ADPCM
		constexpr int kImaSteps[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
			66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
			724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
			4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
			27086, 29794, 32767 };
		constexpr int kImaIndex[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

		struct ImaChannel
		{
			int sample = 0, index = 0;
			int16_t next(int nibble)
			{
				const int step = kImaSteps[index];
				int diff = step >> 3;
				if (nibble & 1)
					diff += step >> 2;
				if (nibble & 2)
					diff += step >> 1;
				if (nibble & 4)
					diff += step;
				sample = std::clamp(nibble & 8 ? sample - diff : sample + diff, -32768, 32767);
				index = std::clamp(index + kImaIndex[nibble & 15], 0, 88);
				return int16_t(sample);
			}
		};

		// Each block: per channel the first sample and the step index, then the channels' nibbles
		// in turns of four bytes (eight samples) each, low nibble first.
		void decodeIma(std::span<const std::byte> data, int channels, size_t blockAlign, Pcm& out)
		{
			if (blockAlign <= size_t(4 * channels))
				return;
			for (size_t at = 0; at + size_t(4 * channels) <= data.size(); at += blockAlign)
			{
				const auto block = data.subspan(at, std::min(blockAlign, data.size() - at));
				ImaChannel ch[2];
				for (int c = 0; c < channels; ++c)
				{
					ch[c].sample = int16_t(uint16_t(uint8_t(block[c * 4]) | (uint8_t(block[c * 4 + 1]) << 8)));
					ch[c].index = std::clamp(int(uint8_t(block[c * 4 + 2])), 0, 88);
					out.samples.push_back(int16_t(ch[c].sample));
				}
				const size_t body = block.size() - size_t(4 * channels), groups = body / size_t(4 * channels);
				std::vector<int16_t> chunk(size_t(8 * channels));
				for (size_t g = 0; g < groups; ++g)
				{
					for (int c = 0; c < channels; ++c)
						for (int b = 0; b < 4; ++b)
						{
							const uint8_t byte = uint8_t(block[size_t(4 * channels) + g * size_t(4 * channels) + size_t(c * 4 + b)]);
							chunk[size_t((b * 2) * channels + c)] = ch[c].next(byte & 15);
							chunk[size_t((b * 2 + 1) * channels + c)] = ch[c].next(byte >> 4);
						}
					out.samples.insert(out.samples.end(), chunk.begin(), chunk.end());
				}
			}
		}

		// ---------------------------------------------------------------- Microsoft ADPCM
		constexpr int kMsAdapt[16] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };

		// Each block: per channel the predictor, the step and the first two samples (the older
		// one first out), then nibbles high first, the channels in turn.
		void decodeMsAdpcm(std::span<const std::byte> data, int channels, size_t blockAlign, const std::vector<std::pair<int, int>>& coefs, Pcm& out)
		{
			const size_t header = size_t(7 * channels);
			if (blockAlign <= header || coefs.empty())
				return;
			const auto s16 = [](std::span<const std::byte> b, size_t at) { return int(int16_t(uint16_t(uint8_t(b[at]) | (uint8_t(b[at + 1]) << 8)))); };
			for (size_t at = 0; at + header <= data.size(); at += blockAlign)
			{
				const auto block = data.subspan(at, std::min(blockAlign, data.size() - at));
				int c1[2], c2[2], delta[2], s1[2], s2[2];
				for (int c = 0; c < channels; ++c)
				{
					const auto& k = coefs[std::min(size_t(uint8_t(block[size_t(c)])), coefs.size() - 1)];
					c1[c] = k.first, c2[c] = k.second;
					delta[c] = s16(block, size_t(channels + 2 * c));
					s1[c] = s16(block, size_t(3 * channels + 2 * c));
					s2[c] = s16(block, size_t(5 * channels + 2 * c));
				}
				for (int c = 0; c < channels; ++c)
					out.samples.push_back(int16_t(s2[c]));
				for (int c = 0; c < channels; ++c)
					out.samples.push_back(int16_t(s1[c]));
				int c = 0;
				for (size_t i = header; i < block.size(); ++i)
					for (const int shift : { 4, 0 })
					{
						const int nibble = (uint8_t(block[i]) >> shift) & 15, signedNibble = nibble >= 8 ? nibble - 16 : nibble;
						const int predicted = (s1[c] * c1[c] + s2[c] * c2[c]) / 256;
						const int16_t sample = clamp16(predicted + signedNibble * delta[c]);
						out.samples.push_back(sample);
						s2[c] = s1[c], s1[c] = sample;
						delta[c] = std::max(16, kMsAdapt[nibble] * delta[c] / 256);
						c = (c + 1) % channels;
					}
			}
		}
	}

	std::optional<Pcm> decodeWav(std::span<const std::byte> file, std::string* error)
	{
		const auto fail = [&](const char* why) -> std::optional<Pcm> {
			if (error)
				*error = why;
			return std::nullopt;
		};
		const Reader r{ file };
		if (!r.has(12) || std::memcmp(file.data(), "RIFF", 4) != 0 || std::memcmp(file.data() + 8, "WAVE", 4) != 0)
			return fail("not a WAVE file");
		uint16_t format = 0, channels = 0, bits = 0, blockAlign = 0;
		uint32_t rate = 0, frames = 0; // (frames: the fact chunk's length of a compressed one)
		std::vector<std::pair<int, int>> coefs;
		std::span<const std::byte> data;
		for (size_t at = 12; at + 8 <= file.size();)
		{
			const uint32_t size = r.u32(at + 4);
			const size_t body = at + 8, length = std::min<size_t>(size, file.size() - body);
			if (std::memcmp(file.data() + at, "fmt ", 4) == 0 && length >= 16)
			{
				format = r.u16(body), channels = r.u16(body + 2), rate = r.u32(body + 4), blockAlign = r.u16(body + 12), bits = r.u16(body + 14);
				if (format == 2 && length >= 22)
				{
					const size_t count = r.u16(body + 20);
					for (size_t i = 0; i < count && body + 22 + i * 4 + 4 <= body + length; ++i)
						coefs.emplace_back(int16_t(r.u16(body + 22 + i * 4)), int16_t(r.u16(body + 24 + i * 4)));
				}
			}
			else if (std::memcmp(file.data() + at, "fact", 4) == 0 && length >= 4)
				frames = r.u32(body);
			else if (std::memcmp(file.data() + at, "data", 4) == 0)
				data = file.subspan(body, length);
			at = body + size + (size & 1);
		}
		if (!channels || channels > 2 || !rate || data.empty())
			return fail("no sound in the WAVE file");
		Pcm out;
		out.rate = rate, out.channels = channels;
		switch (format)
		{
		case 1:
			if (bits == 16)
			{
				out.samples.resize(data.size() / 2);
				std::memcpy(out.samples.data(), data.data(), out.samples.size() * 2);
			}
			else if (bits == 8)
				for (const std::byte b : data)
					out.samples.push_back(int16_t((int(uint8_t(b)) - 128) << 8));
			else
				return fail("unsupported PCM sample size");
			break;
		case 0x11: decodeIma(data, channels, blockAlign, out); break;
		case 2: decodeMsAdpcm(data, channels, blockAlign, coefs, out); break;
		default: return fail("unsupported WAVE format");
		}
		out.samples.resize(out.samples.size() / channels * channels);
		if (format != 1 && frames > 0 && size_t(frames) * channels < out.samples.size())
			out.samples.resize(size_t(frames) * channels); // (the last block's padding)
		return out;
	}

	std::optional<Pcm> decode(std::span<const std::byte> file, std::string* error)
	{
		if (file.size() >= 12 && std::memcmp(file.data(), "RIFF", 4) == 0 && std::memcmp(file.data() + 8, "WAVE", 4) == 0)
			return decodeWav(file, error);
		return decodeMedia(file, error);
	}
}
