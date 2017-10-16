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
#define MAL_NO_NULL
#define MAL_NO_WASAPI
#include "io/input.h"
#include "io/audio/mini_al.h"
#include "libretro-common-master/include/queues/fifo_queue.h"
#include "libretro-common-master/include/rthreads/rthreads.h"

namespace std
{
	typedef wstring        tstring;
	typedef wistringstream tistringstream;
	typedef wostringstream tostringstream;
}

#ifdef __cplusplus
extern "C" {
#endif

	class Audio
	{
	
	public:
	Audio(double srate, double refreshra, double ts)
	{
		skew = ts;
		drc_capac = 512;
		refreshrate = refreshra;
		init(srate);
	}
	~Audio()
	{
		destroy();
	}
	bool init(double sample_rate);
	void destroy();
	void reset();
	bool setRate(double rate);
	void drc();
	void mix(const int16_t* samples, size_t sample_count);
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
	float fps;
	double system_fps;
	double refreshrate;
	double skew;
	double system_rate;

	std::mutex lock;
	std::condition_variable buffer_full;
	std::chrono::time_point<std::chrono::system_clock> start, end;
	double getDeltaMovingAverage();
	void sync_video_tick(void);
	std::vector<double> listDeltaMA;
	int drc_capac;
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
	TCHAR inputcfg_path[MAX_PATH];
	TCHAR corevar_path[MAX_PATH];
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
	DWORD rate;
	bool paused;
	std::list<double> listDeltaMA;
	unsigned frame_count;
	bool running();
	bool loadfile(TCHAR* filename, TCHAR* core_filename, bool gamespecificoptions);
	void splash();
	void render();
	void run();
	void reset();
	bool core_load(TCHAR *sofile,bool specifics, TCHAR* filename, TCHAR* core_filename);
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