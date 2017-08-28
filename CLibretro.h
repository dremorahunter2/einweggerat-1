#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#define OUTSIDE_SPEEX
#include "io/audio/speex_resampler.h"
#define MAL_NO_WASAPI
#include "io/audio/mini_al.h"
#include "libretro-common-master/include/queues/fifo_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

	class Audio
	{
	public:
	bool init(unsigned sample_rate);
	void destroy();
	void reset();
	HANDLE ghWriteEvent;

	bool setRate(double rate);
	void mix(const int16_t* samples, size_t frames);
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


	};

class CLibretro
{
private:
	HWND emulator_hwnd;
	static	CLibretro* m_Instance ;
	
public:
	static CLibretro* CreateInstance(HWND hwnd ) ;
	static	CLibretro* GetSingleton( ) ;
	CLibretro();
	~CLibretro();
	double fps;
	double target_fps;
	double target_samplerate;
	DWORD rate;
	std::list<double> listDeltaMA;
	bool sync_video_tick(void);
	unsigned frame_count;
	double getDeltaMovingAverage(double delta);
	bool running();
	bool loadfile(char* filename);
	void splash();
	void render();
	void run();
	bool init(HWND hwnd);
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