#include "media/Movie.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#ifdef OC4_HAVE_FFMPEG
#ifdef _WIN32
#include <windows.h>
#endif
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#endif

namespace oc4::media
{
	bool available()
	{
#ifdef OC4_HAVE_FFMPEG
		// (the libraries are delay-loaded: nothing of theirs is called unless all three are there)
		#ifdef _WIN32
		static const bool found = LoadLibraryW(L"avutil-60.dll") && LoadLibraryW(L"avcodec-62.dll") && LoadLibraryW(L"avformat-62.dll");
		return found;
#else
		return true; // linked directly
#endif
#else
		return false;
#endif
	}

	struct Movie::Impl
	{
		std::filesystem::path file;
		std::atomic<bool> failed = false, ready = false, ended = false;
		std::shared_ptr<audio::Pcm> sound;

		std::mutex mutex;
		std::condition_variable_any room;
		struct Queued
		{
			double time = 0;
			Frame frame;
		};
		std::deque<Queued> queue;
		Frame current;
		std::jthread worker;

		static constexpr size_t kAhead = 6;

#ifdef OC4_HAVE_FFMPEG
		struct Input
		{
			AVFormatContext* format = nullptr;
			AVCodecContext* codec = nullptr;
			int stream = -1;
			~Input()
			{
				avcodec_free_context(&codec);
				avformat_close_input(&format);
			}
			bool open(const std::filesystem::path& file, AVMediaType type)
			{
				const std::u8string name = file.u8string();
				if (avformat_open_input(&format, reinterpret_cast<const char*>(name.c_str()), nullptr, nullptr) < 0 ||
					avformat_find_stream_info(format, nullptr) < 0)
					return false;
				const AVCodec* decoder = nullptr;
				stream = av_find_best_stream(format, type, -1, -1, &decoder, 0);
				if (stream < 0 || !decoder || !(codec = avcodec_alloc_context3(decoder)) ||
					avcodec_parameters_to_context(codec, format->streams[stream]->codecpar) < 0 || avcodec_open2(codec, decoder, nullptr) < 0)
					return false;
				for (unsigned i = 0; i < format->nb_streams; ++i)
					format->streams[i]->discard = int(i) == stream ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
				return true;
			}
			// Each decoded frame to `take` until it says stop or the stream ends.
			template <typename Take>
			void decode(Take&& take)
			{
				AVPacket* packet = av_packet_alloc();
				AVFrame* frame = av_frame_alloc();
				bool going = true, flushed = false;
				while (going && !flushed)
				{
					if (av_read_frame(format, packet) >= 0)
					{
						if (packet->stream_index == stream)
							avcodec_send_packet(codec, packet);
						av_packet_unref(packet);
					}
					else
					{
						avcodec_send_packet(codec, nullptr);
						flushed = true;
					}
					while (going && avcodec_receive_frame(codec, frame) >= 0)
					{
						going = take(frame);
						av_frame_unref(frame);
					}
				}
				av_frame_free(&frame);
				av_packet_free(&packet);
			}
		};

		static int16_t toShort(float v) { return int16_t(std::clamp(v, -1.0f, 1.0f) * 32767.0f); }

		void readSound(std::stop_token stop)
		{
			Input in;
			if (!in.open(file, AVMEDIA_TYPE_AUDIO))
				return;
			auto pcm = std::make_shared<audio::Pcm>();
			pcm->rate = uint32_t(in.codec->sample_rate);
			pcm->channels = uint16_t(std::clamp(in.codec->ch_layout.nb_channels, 1, 2));
			in.decode([&](const AVFrame* f) {
				const int channels = f->ch_layout.nb_channels, n = f->nb_samples;
				const auto format = AVSampleFormat(f->format);
				for (int i = 0; i < n; ++i)
					for (int c = 0; c < pcm->channels; ++c)
					{
						const int from = std::min(c, channels - 1);
						int16_t v = 0;
						if (format == AV_SAMPLE_FMT_FLTP)
							v = toShort(reinterpret_cast<const float*>(f->extended_data[from])[i]);
						else if (format == AV_SAMPLE_FMT_FLT)
							v = toShort(reinterpret_cast<const float*>(f->extended_data[0])[i * channels + from]);
						else if (format == AV_SAMPLE_FMT_S16P)
							v = reinterpret_cast<const int16_t*>(f->extended_data[from])[i];
						else if (format == AV_SAMPLE_FMT_S16)
							v = reinterpret_cast<const int16_t*>(f->extended_data[0])[i * channels + from];
						pcm->samples.push_back(v);
					}
				return !stop.stop_requested();
			});
			if (!pcm->samples.empty())
				sound = std::move(pcm);
		}

		// YUV 4:2:0 (BT.601, studio range) to BGRA; a fourth plane is its alpha.
		static void convert(const AVFrame* f, Frame& out)
		{
			out.width = uint32_t(f->width), out.height = uint32_t(f->height);
			out.bgra.resize(size_t(out.width) * out.height * 4);
			const bool alpha = f->format == AV_PIX_FMT_YUVA420P;
			for (int y = 0; y < f->height; ++y)
			{
				const uint8_t* py = f->data[0] + ptrdiff_t(y) * f->linesize[0];
				const uint8_t* pu = f->data[1] + ptrdiff_t(y / 2) * f->linesize[1];
				const uint8_t* pv = f->data[2] + ptrdiff_t(y / 2) * f->linesize[2];
				const uint8_t* pa = alpha ? f->data[3] + ptrdiff_t(y) * f->linesize[3] : nullptr;
				uint8_t* o = out.bgra.data() + size_t(y) * out.width * 4;
				for (int x = 0; x < f->width; ++x, o += 4)
				{
					const int c = (py[x] - 16) * 298, d = pu[x / 2] - 128, e = pv[x / 2] - 128;
					o[0] = uint8_t(std::clamp((c + 516 * d + 128) >> 8, 0, 255));
					o[1] = uint8_t(std::clamp((c - 100 * d - 208 * e + 128) >> 8, 0, 255));
					o[2] = uint8_t(std::clamp((c + 409 * e + 128) >> 8, 0, 255));
					if (pa) // (premultiplied, as the overlay draws it)
					{
						o[0] = uint8_t(o[0] * pa[x] / 255), o[1] = uint8_t(o[1] * pa[x] / 255), o[2] = uint8_t(o[2] * pa[x] / 255);
						o[3] = pa[x];
					}
					else
						o[3] = 255;
				}
			}
		}

		void run(std::stop_token stop)
		{
			readSound(stop);
			Input in;
			if (!in.open(file, AVMEDIA_TYPE_VIDEO) || (in.codec->pix_fmt != AV_PIX_FMT_YUV420P && in.codec->pix_fmt != AV_PIX_FMT_YUVA420P))
			{
				failed = true;
				return;
			}
			const AVStream* s = in.format->streams[in.stream];
			const double base = av_q2d(s->time_base), step = s->avg_frame_rate.num ? av_q2d(av_inv_q(s->avg_frame_rate)) : 1.0 / 30;
			uint64_t count = 0;
			in.decode([&](const AVFrame* f) {
				if (f->format != AV_PIX_FMT_YUV420P && f->format != AV_PIX_FMT_YUVA420P)
					return true;
				Queued q;
				q.time = f->best_effort_timestamp != AV_NOPTS_VALUE ? f->best_effort_timestamp * base : double(count) * step;
				convert(f, q.frame);
				q.frame.number = ++count;
				std::unique_lock lock(mutex);
				if (!room.wait(lock, stop, [&] { return queue.size() < kAhead; }))
					return false;
				queue.push_back(std::move(q));
				ready = true;
				return true;
			});
			ended = true;
			ready = true;
		}
#endif
	};

	Movie::Movie(const std::filesystem::path& file) : m(std::make_unique<Impl>())
	{
		m->file = file;
#ifdef OC4_HAVE_FFMPEG
		if (available() && std::filesystem::is_regular_file(file))
		{
			m->worker = std::jthread([this](std::stop_token stop) { m->run(stop); });
			return;
		}
#endif
		m->failed = true;
	}

	Movie::~Movie()
	{
		if (m->worker.joinable())
		{
			m->worker.request_stop();
			m->worker.join();
		}
	}

	bool Movie::failed() const { return m->failed; }
	bool Movie::ready() const { return m->ready && !m->failed; }
	std::shared_ptr<const audio::Pcm> Movie::sound() const { return m->ready ? m->sound : nullptr; }

	const Movie::Frame* Movie::frameAt(double seconds)
	{
		std::lock_guard lock(m->mutex);
		bool taken = false;
		while (!m->queue.empty() && (m->queue.front().time <= seconds || m->current.number == 0))
		{
			m->current = std::move(m->queue.front().frame);
			m->queue.pop_front();
			taken = true;
		}
		if (taken)
			m->room.notify_all();
		return m->current.number ? &m->current : nullptr;
	}

	bool Movie::finished() const
	{
		if (m->failed)
			return true;
		std::lock_guard lock(m->mutex);
		return m->ended && m->queue.empty();
	}
}
