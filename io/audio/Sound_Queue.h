
// Simple sound queue for synchronous sound handling in SDL

// Copyright (C) 2005 Shay Green. MIT license.

#ifndef SOUND_QUEUE_H
#define SOUND_QUEUE_H

#include <windows.h>

#include <mmreg.h> // dsound.h demands this, why is it not included
#include <mmsystem.h> // dsound.h demands this too
#include <dsound.h>
#include <stdint.h>
#include <list>

// Simple SDL sound wrapper that has a synchronous interface
class Sound_Queue {
public:
	Sound_Queue();
	~Sound_Queue();
	
	// Initialize with specified sample rate and channel count.
	// Returns NULL on success, otherwise error string.
	void init(double samplerate, double latency,HWND hwnd);
	void clear();
	void set_samplerate(float rate);

	
	// Write samples to buffer and block until enough space is available
	typedef short sample_t;
	void write( int16_t*, int count );
	void audio_sync();
	size_t write_avail();
	size_t buffer_size();
private:
	
	
	double getDeltaMovingAverage(double delta, int num_iter);
	IDirectSound * ds;
	IDirectSoundBuffer * dsb_p;
	IDirectSoundBuffer * dsb_b;
	DWORD lastwrite;
	DWORD bufsize;
	bool sync;
	HMODULE hDSound;
	double samplerate_d;//only needed for changing latency, but they can be changed separately and in any order
	float orig_ratio;
	double last_adjust;
	
};

#endif

