#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#define OUTSIDE_SPEEX
#include "io/audio/speex_resampler.h"
#define MAL_NO_WASAPI
#include "io/input.h"
#include "io/audio/mini_al.h"
#include "libretro-common-master/include/queues/fifo_queue.h"
#include "libretro-common-master/include/rthreads/rthreads.h"

#ifdef __cplusplus
extern "C" {
#endif

	class Audio
	{
	
	public:
	Audio(double srate)
	{
		init(srate);
	}
	~Audio()
	{
		destroy();
	}
	bool init(unsigned sample_rate);
	void destroy();
	void reset();
	void mix(const int16_t* samples, size_t frames,int64_t fps);
	mal_uint32 fill_buffer(uint8_t* pSamples, mal_uint32 samplecount);
	mal_context context;
	mal_device device;
	mal_device_config config;
	unsigned _sampleRate;
	unsigned _coreRate;
	SpeexResamplerState* _resampler;
	fifo_buffer* _fifo;
	bool           _opened;
	bool           _mute;
	float          _scale;
	const int16_t* _samples;
	size_t         _frames;
	std::mutex lock;
	bool buf_ready;
	std::condition_variable buffer_full;
	std::condition_variable buffer_empty;
	};

class CLibretro
{
private:
	
	static	CLibretro* m_Instance ;
	
public:
	struct core_vars
	{
		std::string name;
		std::string var;
		std::string description;
		std::string usevars;
	};
	std::vector<core_vars> variables;
	bool variables_changed;
	HANDLE thread_handle;
	DWORD thread_id;
	HWND emulator_hwnd;
	static DWORD WINAPI libretro_thread(void* Param);
	static CLibretro* CreateInstance(HWND hwnd ) ;
	static	CLibretro* GetSingleton( ) ;
	CLibretro();
	~CLibretro();
	double fps;
	double target_fps;
	double target_samplerate;
	DWORD rate;
	bool paused;
	std::list<double> listDeltaMA;
	bool sync_video_tick(void);
	unsigned frame_count;
	double getDeltaMovingAverage(double delta);
	bool running();
	bool loadfile(char* filename);
	void splash();
	void render();
	void run();
	void reset();
	bool init(HWND hwnd);
	bool savestate(TCHAR* filename, bool save = false);
	void kill();
	BOOL isEmulating;

	void core_audio_sample(int16_t left, int16_t right);
	size_t core_audio_sample_batch(const int16_t *data, size_t frames);

	int16_t*                        _samples;
	size_t                          _samplesCount;
	Audio * _audio;
};

#ifdef __cplusplus
}
#endif

#endif CEMULATOR_H