
// Nes_Snd_Emu 0.1.7. http://www.slack.net/~ant/

#include "Sound_Queue.h"

#include <assert.h>
#include <string.h>


/* Copyright (C) 2005 by Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
static const char* sdl_error(const char* str)
{
//	const char* sdl_str = SDL_GetError();
	//if (sdl_str && *sdl_str)
	//	str = sdl_str;
	return "";
}

Sound_Queue::Sound_Queue()
{
	hDSound = NULL;
	hDSound = LoadLibrary(L"dsound.dll");
}

Sound_Queue::~Sound_Queue()
{
	if (dsb_b) { dsb_b->Stop(); dsb_b->Release(); }
	if (dsb_p) { dsb_p->Stop(); dsb_p->Release(); }
	if (ds) { ds->Release(); }

	FreeLibrary(hDSound);
}





void Sound_Queue::audio_sync()
{
	
}



size_t Sound_Queue::write_avail()
{
	size_t avail = 0;
	DWORD freeend = 0;
	DWORD freestart = 0;
	DWORD thispos = lastwrite;
	dsb_b->GetCurrentPosition(&freeend, &freestart);
	freestart /= 4; freeend /= 4;

	size_t first = freestart;
	size_t end = freeend;

	if (thispos < freestart) thispos +=bufsize;
	if (end < thispos) end += bufsize;
	if (end < thispos) end += bufsize;
	avail = end - thispos ;
	return avail;
}

size_t Sound_Queue::buffer_size()
{
	return bufsize;
}

void Sound_Queue::write( int16_t* samples, int numframes )
{

	DWORD freeend;
	DWORD freestart;
	DWORD thispos = lastwrite;
wait:
	dsb_b->GetCurrentPosition( &freeend, &freestart);
	freestart /= 4; freeend /= 4;
	if (thispos < freestart) thispos += bufsize;
	if (freeend < thispos) freeend += bufsize;

	if (thispos + numframes>freeend)
	{
		//if (this->sync) { Sleep(1); goto wait; }else
		//Sleep(); goto wait;
	numframes = freeend - thispos;
	}


	void* data1; DWORD data1bytes;
	void* data2; DWORD data2bytes;
	if (dsb_b->Lock( thispos%bufsize * 4, numframes * 4, &data1, &data1bytes, &data2, &data2bytes, 0) == DS_OK)
	{
		memcpy(data1, samples, data1bytes);
		memcpy(data2, samples + data1bytes / 4 * 2, data2bytes);
		dsb_b->Unlock( data1, data1bytes, data2, data2bytes);
		lastwrite = (thispos + (data1bytes + data2bytes) / 4) % bufsize;
	}
}

void Sound_Queue::clear()
{
	if (!dsb_b) return;
	dsb_b->Stop();
	dsb_b->SetCurrentPosition( 0);

	DWORD size;
	void* buffer;
	dsb_b->Lock( 0, -1, &buffer, &size, 0, 0, DSBLOCK_ENTIREBUFFER);
	memset(buffer, 0, size);
	dsb_b->Unlock( buffer, size, 0, 0);

	dsb_b->Play( 0, 0, DSBPLAY_LOOPING);

	lastwrite = 0;

}

void Sound_Queue::set_samplerate(float rate)
{
	samplerate_d = samplerate_d;
	dsb_b->SetFrequency( samplerate_d);
}

void Sound_Queue::init(double samplerate, double latency, HWND hwnd)
{
	
	//lpDirectSoundCreate=DirectSoundCreate;//this is for type checking; it's not needed anymore
	HRESULT(WINAPI * lpDirectSoundCreate)(LPCGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
	lpDirectSoundCreate = (HRESULT(WINAPI*)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN))GetProcAddress(hDSound, "DirectSoundCreate");
	sync = true;
	last_adjust = 0;

	samplerate_d = samplerate;
	const long samplerates[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000 };
	for (int i = 0; i < 7; i++)
	{
		if (samplerate == samplerates[i])
		{
			samplerate_d = samplerate;
			break;
		}
	}

	lpDirectSoundCreate(NULL, &ds, NULL);
	ds->SetCooperativeLevel((HWND)hwnd, DSSCL_PRIORITY);

	bufsize = samplerate_d*latency / 1000.0 + 0.5;

	DSBUFFERDESC dsbd;
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbd.dwBufferBytes = 0;
	dsbd.lpwfxFormat = 0;
	ds->CreateSoundBuffer( &dsbd, &dsb_p, 0);

	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = samplerate_d;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.wBitsPerSample / 8 * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
	dsbd.dwBufferBytes = bufsize * sizeof(uint32_t);
	dsbd.guid3DAlgorithm = GUID_NULL;
	dsbd.lpwfxFormat = &wfx;
	ds->CreateSoundBuffer( &dsbd, &dsb_b, 0);
	dsb_b->SetFrequency( samplerate);
	dsb_b->SetCurrentPosition( 0);
	clear();
}

