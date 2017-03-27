#include "stdafx.h"
#include <windows.h>
#include "CLibretro.h"
#include "libretro.h"
#include "io/gl_render.h"
#include "gui/utf8conv.h"
#include "io/audio/Sound_Queue.h"
using namespace std;
using namespace utf8util;

static struct {
	HMODULE handle;
	bool initialized;

	void(*retro_init)(void);
	void(*retro_deinit)(void);
	unsigned(*retro_api_version)(void);
	void(*retro_get_system_info)(struct retro_system_info *info);
	void(*retro_get_system_av_info)(struct retro_system_av_info *info);
	void(*retro_set_controller_port_device)(unsigned port, unsigned device);
	void(*retro_reset)(void);
	void(*retro_run)(void);
	//	size_t retro_serialize_size(void);
	//	bool retro_serialize(void *data, size_t size);
	//	bool retro_unserialize(const void *data, size_t size);
	//	void retro_cheat_reset(void);
	//	void retro_cheat_set(unsigned index, bool enabled, const char *code);
	bool(*retro_load_game)(const struct retro_game_info *game);
	//	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void(*retro_unload_game)(void);
	//	unsigned retro_get_region(void);
	//	void *retro_get_memory_data(unsigned id);
	//	size_t retro_get_memory_size(unsigned id);
} g_retro;

static void core_unload() {
	if (g_retro.initialized)
	g_retro.retro_deinit();
	if (g_retro.handle)
	{
		FreeLibrary(g_retro.handle);
		g_retro.handle = NULL;
	}
	
}

static void core_log(enum retro_log_level level, const char *fmt, ...) {
	char buffer[4096] = { 0 };
	char buffer2[4096] = { 0 };
	static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
	va_list va;

	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (level == 0)
		return;

	sprintf(buffer2, "[%s] %s", levelstr[level], buffer);
	wstring wtf = utf16_from_utf8(buffer2);
	OutputDebugString(wtf.c_str());

	if (level == RETRO_LOG_ERROR)
		exit(EXIT_FAILURE);
}


uintptr_t core_get_current_framebuffer() {
	return g_video.fbo_id;
}

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

TCHAR* GetThisPath(TCHAR* dest, size_t destSize)
{
	if (!dest) return NULL;
	if (MAX_PATH > destSize) return NULL;

	DWORD length = GetModuleFileName(NULL, dest, destSize);
	PathRemoveFileSpec(dest);
	return dest;
}

bool core_environment(unsigned cmd, void *data) {
	bool *bval;
	TCHAR syspath[512] = { 0 };
	GetThisPath(syspath, 512);
	lstrcat(syspath, L":\\system");
	TCHAR savepath[512] = { 0 };
	GetThisPath(savepath, 512);
	lstrcat(savepath, L"\\saves\\");
	string ansi = ansi_from_utf16(syspath);
	string ansi2 = utf8_from_utf16(savepath);
	char *sys_path = strdup(ansi.c_str());
	char *save_path = NULL;

	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
		struct retro_log_callback *cb = (struct retro_log_callback *)data;
		cb->log = core_log;
		return true;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		bval = (bool*)data;
		*bval = true;
		return true;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		//sys_path = strdup(ansi.c_str());
		*(char**)data = sys_path;
		break;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		save_path = strdup(ansi.c_str());
		*(char**)data = save_path;
		break;

	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;
		if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
			return false;
		return video_set_pixel_format(*fmt);
	}
	case RETRO_ENVIRONMENT_SET_HW_RENDER: {
		struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
		hw->get_current_framebuffer = core_get_current_framebuffer;
		hw->get_proc_address = (retro_hw_get_proc_address_t)wglGetProcAddress;
		g_video.hw = *hw;
		return true;
	}
	default:
		core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
		return false;
	}

	return false;
}

static void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
	video_refresh(data, width, height, pitch);
}


static void core_input_poll(void) {
	
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port || index || device != RETRO_DEVICE_JOYPAD)
		return 0;

	return 0;
}


Sound_Queue* soundQueue;

static void core_audio_sample(int16_t left, int16_t right) {
	int16_t buf[2] = { left, right };
	soundQueue->write(buf, 1);

//	WindowsAudio::Write((unsigned char*)buf, sizeof(*buf) * 2);
}


static size_t core_audio_sample_batch(const int16_t *data, size_t frames) {
	//return audio_write(data, frames);


	soundQueue->write((int16_t*)data,frames);
	return frames;
	//WindowsAudio::Write((unsigned char*)data, frames * 2 * sizeof(*data) );
}

bool core_load(TCHAR *sofile) {
	void(*set_environment)(retro_environment_t) = NULL;
	void(*set_video_refresh)(retro_video_refresh_t) = NULL;
	void(*set_input_poll)(retro_input_poll_t) = NULL;
	void(*set_input_state)(retro_input_state_t) = NULL;
	void(*set_audio_sample)(retro_audio_sample_t) = NULL;
	void(*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
	memset(&g_retro, 0, sizeof(g_retro));
	g_retro.handle = LoadLibrary(sofile);

#define die() do { FreeLibrary(g_retro.handle); return false; } while(0)
#define libload(name) GetProcAddress(g_retro.handle, name)
#define load(name) if (!(*(void**)(&g_retro.#name)=(void*)libload(#name))) die()
#define load_sym(V,name) if (!(*(void**)(&V)=(void*)libload(#name))) die()
#define load_retro_sym(S) load_sym(g_retro.S, S)
	load_retro_sym(retro_init);
	load_retro_sym(retro_deinit);
	load_retro_sym(retro_api_version);
	load_retro_sym(retro_get_system_info);
	load_retro_sym(retro_get_system_av_info);
	load_retro_sym(retro_set_controller_port_device);
	load_retro_sym(retro_reset);
	load_retro_sym(retro_run);
	load_retro_sym(retro_load_game);
	load_retro_sym(retro_unload_game);

	load_sym(set_environment, retro_set_environment);
	load_sym(set_video_refresh, retro_set_video_refresh);
	load_sym(set_input_poll, retro_set_input_poll);
	load_sym(set_input_state, retro_set_input_state);
	load_sym(set_audio_sample, retro_set_audio_sample);
	load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);
	//set libretro func pointers
	set_environment(core_environment);
	set_video_refresh(core_video_refresh);
	set_input_poll(core_input_poll);
	set_input_state(core_input_state);
	set_audio_sample(core_audio_sample);
	set_audio_sample_batch(core_audio_sample_batch);

	g_retro.retro_init();
	g_retro.initialized = true;
}

static void noop() { int i = 20;
i++;
}



CLibretro* CLibretro::m_Instance = 0 ;
CLibretro* CLibretro::CreateInstance(HWND hwnd )
{
	if (0 == m_Instance)
	{
		m_Instance = new CLibretro( ) ;
		m_Instance->init( hwnd) ;
	}
	return m_Instance ;
}

CLibretro* CLibretro::GetSingleton( )
{
	return m_Instance ;
}
//////////////////////////////////////////////////////////////////////////////////////////

bool CLibretro::running()
{
	return isEmulating;
}

CLibretro::CLibretro()
{
	isEmulating = false;
}

CLibretro::~CLibretro(void)
{
	kill();
}


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

bool CLibretro::loadfile(char* filename)
{
	retro_system_av_info av = { 0 };
	struct retro_system_info system = {0};
	struct retro_game_info info = { filename, 0 };

	g_video = { 0 };
//	g_video.hw.version_major = 3;
//	g_video.hw.version_minor = 3;
//	g_video.hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
	g_video.hw.context_reset = ::noop;
	g_video.hw.context_destroy = ::noop;

	core_load(L"snes9x2005_libretro.dll");

	FILE *Input = fopen(filename, "rb");
	if (!Input) return(NULL);
	// Get the filesize
	fseek(Input, 0, SEEK_END);
	int Size = ftell(Input);
	fseek(Input, 0, SEEK_SET);
	BYTE *Memory = (BYTE *)malloc(Size);
	if (!Memory) return(NULL);
	if (fread(Memory, 1, Size, Input) != (size_t)Size) return(NULL);
	if (Input) fclose(Input);
	Input = NULL;

	info.path = filename;
	info.data = NULL;
	info.size = Size;
	info.meta = NULL;

	g_retro.retro_get_system_info(&system);
	if (!system.need_fullpath) {
		info.data = malloc(info.size);
		memcpy((BYTE*)info.data, Memory, info.size);
		free(Memory);
	}

	if (!g_retro.retro_load_game(&info)) return false;
	g_retro.retro_get_system_av_info(&av);
	::video_configure(&av.geometry,emulator_hwnd);
	//audio_init(av.timing.sample_rate);
	//WindowsAudio::Open(2, av.timing.sample_rate);

	soundQueue = new Sound_Queue;
	soundQueue->init(av.timing.sample_rate, 120, emulator_hwnd);


	if (info.data)
	free((void*)info.data);


	framecount = 0;
	rateticks = 1000 / av.timing.fps;
	baseticks = milliseconds_now();
	baseticks_base = baseticks;
	lastticks = baseticks;
	rate = av.timing.fps;
	isEmulating = true;
	listDeltaMA.clear();
	target_fps = av.timing.fps;
	target_samplerate = av.timing.sample_rate;
	return isEmulating;
}

void CLibretro::splash()
{
	if (!isEmulating){
		PAINTSTRUCT ps;
		HDC pDC = BeginPaint(emulator_hwnd,&ps);
		RECT rect;
		GetClientRect(emulator_hwnd, &rect);
		HBRUSH hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
		::FillRect(pDC, &rect, hBrush);
		EndPaint(emulator_hwnd, &ps);
	}
}



double CLibretro::getDeltaMovingAverage(double delta, int num_iter)
{
	//if (listDeltaMA.size() == num_iter) listDeltaMA.clear();
	listDeltaMA.push_back(delta);
	if (listDeltaMA.size() >=num_iter) listDeltaMA.pop_front();
	double sum = 0;
	for (std::list<double>::iterator p = listDeltaMA.begin(); p != listDeltaMA.end(); ++p)
		sum += (double)*p;
	return sum / listDeltaMA.size();
}

void CLibretro::run()
{
	if(isEmulating)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		g_retro.retro_run();

		soundQueue->audio_sync();
		/*
		if (orig_ratio != 1.0)
		{
			size_t available = write_avail();
			size_t total = buffer_size();
			int half = total / 2;
			int delta_half = available - half;
			double adjust = 1.0 + 0.005 * ((double)delta_half / half);

			unsigned low_water_size = total - (unsigned)(total * 3 / 4);
			double avg = getDeltaMovingAverage(adjust, write_avail());
			if (avg != last_adjust && available < low_water_size)
			{
				set_samplerate(samplerate_d*avg);
				last_adjust = avg;
			}
		}*/

		

		DWORD current_ticks = milliseconds_now();
		
		DWORD target_ticks = baseticks + (DWORD)((float)framecount * rateticks);
		framecount++;
		
		if (current_ticks <= target_ticks) {
			DWORD the_delay = target_ticks - current_ticks;
			Sleep(the_delay);
		}
	//	else {
		//	framecount = 0;
		//	baseticks = milliseconds_now();
		//}
		double fps = ((double)framecount * 1000) / (current_ticks - baseticks_base);
		double avg = getDeltaMovingAverage(fps, 2048);
		TCHAR str[256] = { 0 };
		swprintf(str, L"[%f fps] [%f mean fps] [%f target fps]", fps, avg, target_fps);
		SetWindowText(emulator_hwnd, str);

		size_t available = soundQueue->write_avail();
		size_t total = soundQueue->buffer_size();
		unsigned low_water_size = total - (unsigned)(total * 3 / 4);

		if (avg < 60 && available < low_water_size) {
			double sampleRate = target_samplerate * avg / target_fps;
			soundQueue->set_samplerate(sampleRate);
		}


	}
}
bool CLibretro::init(HWND hwnd)
{
	isEmulating = false;
	emulator_hwnd = hwnd;
	return true;
}

void CLibretro::kill()
{
	isEmulating = false;
	//WindowsAudio::Close();
}

