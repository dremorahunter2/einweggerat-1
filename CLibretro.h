#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <audio/audio_resampler.h>
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

	void *resampler_sinc_init(double bandwidth_mod);
	void resampler_sinc_process(void *re_, struct resampler_data *data);
	void resampler_sinc_free(void *re_);

	class Audio
	{
	
	public:
	bool init(double refreshra);
	void destroy();
	void reset();
	void sleeplil();
	void mix(const int16_t* samples, size_t sample_count);
	mal_uint32 fill_buffer(uint8_t* pSamples, mal_uint32 samplecount);
	mal_context context;
	mal_device device;
	unsigned client_rate;
	fifo_buffer* _fifo;
	float fps;
	double system_fps;
	double skew;
	double system_rate;
	double resamp_original;
	void* resample;
	float *input_float;
	float *output_float;

	std::mutex lock;
	std::condition_variable buffer_full;
	};

class CLibretro
{
private:
	
	static	CLibretro* m_Instance ;
	
public:
	struct core_vars
	{
		char name[100];
		char var[100];
	    char description[256];
		char usevars[256];
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
	Audio  _audio;

	double lastTime;
    int nbFrames;
};

#ifdef __cplusplus
}
#endif

#endif CEMULATOR_H