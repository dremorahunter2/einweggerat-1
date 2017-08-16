#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#define OUTSIDE_SPEEX
#include "io/audio/speex_resampler.h"
#define MAL_NO_WASAPI
#include "io/audio/mini_al.h"

#ifdef __cplusplus
extern "C" {
#endif

	struct fifo_buffer
	{
		uint8_t *buffer;
		size_t size;
		size_t first;
		size_t end;
	};
	typedef struct fifo_buffer fifo_buffer_t;

	class Audio
	{
	public:
	bool init(unsigned sample_rate);
	void destroy();
	void reset();

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
	DWORD framecount;
	float rateticks;
	DWORD baseticks;
	DWORD baseticks_base;
	DWORD lastticks;
	DWORD rate;
	std::list<double> listDeltaMA;
	double getDeltaMovingAverage(double delta, int num_iter);
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