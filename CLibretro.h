#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#include "io/audio/minisdl_audio.h"
#define OUTSIDE_SPEEX
#include "io/audio/speex_resampler.h"


#ifdef __cplusplus
extern "C" {
#endif


	class Fifo
	{
	public:
		bool init(size_t size);
		void destroy();
		void reset();

		void read(void* data, size_t size);
		void write(const void* data, size_t size);

		size_t occupied();
		size_t free();

		SDL_mutex* _mutex;
		uint8_t*   _buffer;
		size_t     _size;
		size_t     _avail;
		size_t     _first;
		size_t     _last;
	};


	class Audio
	{
	public:
	bool init(unsigned sample_rate);
	void destroy();
	void reset();

	bool setRate(double rate);
	void mix(const int16_t* samples, size_t frames);
	void fill_buffer(Uint8* out, int count);
	static void sdl_audio_callback(void* user_data, Uint8* out, int count);

	SDL_AudioSpec     _audioSpec;
	SDL_AudioDeviceID _audioDev;
	unsigned _sampleRate;
	unsigned _coreRate;
	SpeexResamplerState* _resampler;
	Fifo* _fifo;
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