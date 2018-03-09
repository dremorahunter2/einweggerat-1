#define MAL_IMPLEMENTATION
#include "audio.h"
using namespace std;

#define SAMPLE_COUNT (1024 * 4)

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
	client_rate = 44100;
	resamp_original = (client_rate / system_rate);
	_fifo = fifo_new(SAMPLE_COUNT * sizeof(float)); //number of bytes
	output_float = new float[SAMPLE_COUNT * 4]; //spare space for resampler
	input_float = new float[SAMPLE_COUNT];
	

	resample = resampler_sinc_init(resamp_original);
	if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
		printf("Failed to initialize context.");
		return false;
	}
	mal_device_config config = mal_device_config_init_playback(mal_format_f32, 2, client_rate, audio_callback);
	config.bufferSizeInFrames = SAMPLE_COUNT / 2;
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

void Audio::mix(const int16_t* samples, size_t size)
{
	uint32_t in_len = size * 2;
	int    half_size = (int)_fifo->size / 2;
	double drc_ratio = resamp_original * (1.0 + 0.005 * (((int)fifo_write_avail(_fifo) - half_size) / (double)half_size));
	mal_pcm_s16_to_f32(input_float, samples, in_len);

	struct resampler_data src_data = { 0 };
	src_data.input_frames = size;
	src_data.ratio = drc_ratio;
	src_data.data_in = input_float;
	src_data.data_out = output_float;
	resampler_sinc_process(resample, &src_data);
	int out_len = src_data.output_frames * 2 * sizeof(float);

	size_t written = 0;
	while (written < out_len)
	{
		size_t avail = fifo_write_avail(_fifo);
		if (avail)
		{
			size_t write_amt = out_len - written > avail ? avail : out_len - written;
			fifo_write(_fifo, (const char*)output_float + written, write_amt);
			written += write_amt;
		}
	}
}

mal_uint32 Audio::fill_buffer(uint8_t* out, mal_uint32 count) {
	size_t avail = fifo_read_avail(_fifo);
	size_t write_size = count > avail ? avail : count;
	fifo_read(_fifo, out, write_size);
	size_t silence = count - write_size;
	if(silence)memset(out + write_size, 0,silence );
	return count;
}

