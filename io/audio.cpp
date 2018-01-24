#include "audio.h"
using namespace std;
static mal_uint32 audio_callback(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
	//convert from samples to the actual number of bytes.
	int count_bytes = frameCount * pDevice->channels * mal_get_sample_size_in_bytes(mal_format_f32);
	int ret = ((Audio*)pDevice->pUserData)->fill_buffer((uint8_t*)pSamples, count_bytes) / mal_get_sample_size_in_bytes(mal_format_f32);
	return ret / pDevice->channels;
	//...and back to samples
}

static retro_time_t frame_limit_minimum_time = 0.0;
static retro_time_t frame_limit_last_time = 0.0;

long long milliseconds_now() {
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (1000LL * now.QuadPart) / s_frequency.QuadPart;
	}
	else {
		return GetTickCount();
	}
}

long long microseconds_now() {
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (1000000LL * now.QuadPart) / s_frequency.QuadPart;
	}
	else {
		return GetTickCount();
	}
}

bool Audio::init(double refreshra, retro_system_av_info av)
{
	system_rate = av.timing.sample_rate;
	system_fps = av.timing.fps;
	skew = fabs(1.0f - system_fps / refreshra);
	if (skew >= 0.005)skew = 0.005;
	if (skew <= 0.005)
	{
		system_rate *= ((double)refreshra / system_fps);
	}

	client_rate = 44100;
	resamp_original = (client_rate / system_rate);

	output_float = new float[SAMPLE_COUNT * 4];
	input_float = new float[SAMPLE_COUNT];

	resample = resampler_sinc_init(resamp_original);
	if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
		printf("Failed to initialize context.");
		return false;
	}
	mal_device_config config = mal_device_config_init_playback(mal_format_f32, 2, client_rate, audio_callback);
	config.bufferSizeInFrames = 2048;
	_fifo = fifo_new(SAMPLE_COUNT);
	if (mal_device_init(&context, mal_device_type_playback, NULL, &config, this, &device) != MAL_SUCCESS) {
		mal_context_uninit(&context);
		return false;
	}
	mal_device_start(&device);
	frame_limit_last_time = microseconds_now();
	frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f / (av.timing.fps));
	return true;
}
void Audio::destroy()
{
	{
		mal_device_stop(&device);
		mal_context_uninit(&context);
		delete[] input_float;
		delete[] output_float;
		resampler_sinc_free(resample);
	}
}
void Audio::reset()
{
}


void Audio::sleeplil()
{
	retro_time_t to_sleep_ms = ((frame_limit_last_time + frame_limit_minimum_time) - microseconds_now()) / 1000;
	if (to_sleep_ms > 0)
	{
		float sleep_ms = (unsigned)to_sleep_ms;
		/* Combat jitter a bit. */
		frame_limit_last_time += frame_limit_minimum_time;
		Sleep(sleep_ms);
		return;
	}
	frame_limit_last_time = microseconds_now();
}

void Audio::mix(const int16_t* samples, size_t frames)
{
	uint32_t in_len = frames * 2;
	int available = fifo_write_avail(_fifo);
	double drc_ratio = resamp_original *  (1.0 + skew * ((double)(available - SAMPLE_COUNT * 2) / SAMPLE_COUNT));
	mal_pcm_s16_to_f32(input_float, samples, in_len);

	struct resampler_data src_data = { 0 };
	src_data.input_frames = frames;
	src_data.ratio = drc_ratio;
	src_data.data_in = input_float;
	src_data.data_out = output_float;
	resampler_sinc_process(resample, &src_data);
	int out_len = src_data.output_frames * 2 * sizeof(float);

	std::unique_lock<std::mutex> l(lock);
	buffer_full.wait(l, [this, out_len]() {return out_len < fifo_write_avail(_fifo); });

	fifo_write(_fifo, output_float, out_len);
}

mal_uint32 Audio::fill_buffer(uint8_t* out, mal_uint32 count) {
	std::lock_guard<std::mutex> lg(mutex);
	size_t avail = fifo_read_avail(_fifo);
	size_t write_size = count > avail ? avail : count;
	buffer_full.notify_all();
	fifo_read(_fifo, out, write_size);
	memset(out + write_size, 0, count - write_size);
	return count;
}

