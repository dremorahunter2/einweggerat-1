#include "stdafx.h"
#include <windows.h>
#define OUTSIDE_SPEEX
#define MAL_IMPLEMENTATION
#define MAL_NO_WASAPI
#include "CLibretro.h"
#include "libretro.h"
#include "io/gl_render.h"
#include "gui/utf8conv.h"
#define INI_IMPLEMENTATION
#include "ini.h"


using namespace std;
using namespace utf8util;

#define SAMPLE_COUNT 8192
#define INLINE 

static mal_uint32 sdl_audio_callback(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
	//convert from samples to the actual number of bytes.
	int count_bytes = frameCount * pDevice->channels * mal_get_sample_size_in_bytes(mal_format_s16);
	int ret = ((Audio*)pDevice->pUserData)->fill_buffer((uint8_t*)pSamples, count_bytes) / mal_get_sample_size_in_bytes(mal_format_s16);
	return ret / pDevice->channels;
	//...and back to samples
}

mal_uint32 Audio::fill_buffer(uint8_t* out, mal_uint32 count) {
	std::unique_lock<std::mutex> lck(lock);
	size_t avail = fifo_read_avail(_fifo);
	if (avail)
	{
		avail = count > avail ? avail : count;
		fifo_read(_fifo, out, avail);
		memset(out + avail, 0, count - avail);
		lck.unlock();
		buffer_full.notify_all();
		return avail;
	}
	return 0;
}
	bool Audio::init(unsigned sample_rate)
	{
		_opened = true;
		_mute = false;
		_scale = 1.0f;
		_samples = NULL;
		_coreRate = 0;
		_resampler = NULL;
		_sampleRate = 44100;
		setRate(sample_rate);
		if (mal_context_init(NULL, 0, &context) != MAL_SUCCESS) {
			printf("Failed to initialize context.");
			return false;
		}
		config = mal_device_config_init_playback(mal_format_s16, 2,_sampleRate, ::sdl_audio_callback);
		config.bufferSizeInFrames = 1024;
		_fifo = fifo_new(SAMPLE_COUNT);
		if (mal_device_init(&context, mal_device_type_playback, NULL, &config, this, &device) != MAL_SUCCESS) {
			mal_context_uninit(&context);
			return false;
		}
		mal_device_start(&device);
		return true;
	}
	void Audio::destroy()
	{
		{
			mal_device_stop(&device);
			mal_context_uninit(&context);
			if (_resampler != NULL)
			{
				speex_resampler_destroy(_resampler);
			}
		}
	}
	void Audio::reset()
	{

	}

	bool Audio::setRate(double rate)
	{
		if (_resampler != NULL)
		{
			speex_resampler_destroy(_resampler);
			_resampler = NULL;
		}

		_coreRate = floor(rate + 0.5);

		if (_coreRate != _sampleRate)
		{
			int error;
			_resampler = speex_resampler_init(2, _coreRate, _sampleRate, SPEEX_RESAMPLER_QUALITY_DESKTOP, &error);
		}
		return true;

	}
	void Audio::mix(const int16_t* samples, size_t frames)
	{
		//_samples = samples;
		_frames = frames;
		uint32_t in_len = frames * 2;
		uint32_t out_len = trunc((uint32_t)(in_len * _sampleRate / _coreRate))+1;

		int16_t* output = (int16_t*)alloca(out_len * 2);
		
		if (output == NULL)
		{
			return;
		}

		if (_resampler != NULL)
		{
			int error = speex_resampler_process_int(_resampler, 0, samples, &in_len, output, &out_len);
		}
		else
		{
			memcpy(output, samples, out_len * 2);
		}
		
		int size = out_len * 2;
		std::unique_lock<std::mutex> l(lock);
		buffer_full.wait(l, [this, size]() {return size < fifo_write_avail(_fifo); });
		
		fifo_write(_fifo, output, size);
	}



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
	fprintf(stdout, "%s",buffer2);
}


uintptr_t core_get_current_framebuffer() {
	return g_video.fbo_id;
}

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

void GetThisPath(TCHAR* dest, size_t destSize)
{
	TCHAR Buffer[512];
	DWORD dwRet;
	dwRet = GetCurrentDirectory(512, dest);
}

void init_coresettings(retro_variable *var)
{
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(g_retro.handle, buffer, MAX_PATH);
	PathRemoveExtension(buffer);
	lstrcat(buffer, _T(".ini"));
	FILE *fp = NULL;
	fp = _wfopen(buffer, L"r");
	if (!fp)
	{
		//create a new file with defaults
		ini_t* ini = ini_create(NULL);
		while (var->key && var->value)
		{
			char * pch = strstr((char*)var->value, (char*)"; ");
			pch += strlen("; ");
			int vars = 0;

			while (pch != NULL)
			{
				char val[255] = { 0 };
				char* str2 = strstr(pch, (char*)"|");
				if (!str2)
				{
					strcpy(val, pch);
					break;
				}
				strncpy(val, pch, str2 - pch);
				if (!vars) {
					ini_property_add(ini, INI_GLOBAL_SECTION, var->key, strlen(var->key), val, strlen(val));
					char val_comment[255] = { 0 };
					strcpy(val_comment, var->key);
					strcat(val_comment, ".usedvars");
					ini_property_add(ini, INI_GLOBAL_SECTION, val_comment, strlen(val_comment), pch, strlen(pch));
				}
				pch += str2 - pch++;
				vars++;
			}
			++var;
		}
		int size = ini_save(ini, NULL, 0); // Find the size needed
		char* data = (char*)malloc(size);
		size = ini_save(ini, data, size); // Actually save the file
		ini_destroy(ini);

		fp = _wfopen(buffer, L"w");
		fwrite(data, 1, size, fp);
		fclose(fp);
		free(data);
	}
	else
	{
		fclose(fp);
	}
}
const char* load_coresettings(retro_variable *var)
{
	char variable_val2[50] = { 0 };
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(g_retro.handle, buffer, MAX_PATH);
	PathRemoveExtension(buffer);
	lstrcat(buffer, _T(".ini"));
	FILE *fp = NULL;
	fp = _wfopen(buffer, L"r");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* data = (char*)malloc(size + 1);
	fread(data, 1, size, fp);
	data[size] = '\0';
	fclose(fp);
	ini_t* ini = ini_load(data,NULL);
	free(data);
	int second_index = ini_find_property(ini, INI_GLOBAL_SECTION, (char*)var->key,strlen(var->key));
	const char* variable_val = ini_property_value(ini, INI_GLOBAL_SECTION, second_index);
	if (variable_val == NULL)
	{
		ini_destroy(ini);
		return NULL;
	}
	strcpy(variable_val2, variable_val);
	ini_destroy(ini);
	return variable_val2;
}

std::wstring s2ws(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

bool core_environment(unsigned cmd, void *data) {
	bool *bval;
	char *sys_path = "system";

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
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: // 9
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: // 31
	{
		char **ppDir = (char**)data;
		*ppDir = (char*)sys_path;
		return true;
	}
	break;

	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: // 31
	{
		input *input_device = input::GetSingleton();

		char variable_val2[50] = { 0 };
		TCHAR buffer[MAX_PATH] = { 0 };
		GetModuleFileName(g_retro.handle, buffer, MAX_PATH);
		PathRemoveExtension(buffer);
		lstrcat(buffer, L"_input.cfg");
		Std_File_Reader_u out;
		lstrcpy(input_device->path, buffer);
		if (!out.open(buffer))
		{
			input_device->load(out);
			out.close();
		}
		else
		{
			struct retro_input_descriptor *var = (struct retro_input_descriptor *)data;
			static int i = 0;
			while (var != NULL && var->port == 0)
			{
				
				dinput::di_event keyboard;
				keyboard.type = dinput::di_event::ev_none;
				keyboard.key.type = dinput::di_event::key_none;
				keyboard.key.which = NULL;
				CString str = var->description;
			    int id = var->id;
				if (var->device == RETRO_DEVICE_ANALOG)
				{
					if (var->index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
					{
						if (var->id == RETRO_DEVICE_ID_ANALOG_X)id = 16;
						if (var->id == RETRO_DEVICE_ID_ANALOG_Y)id = 17;
						input_device->bl->add(keyboard, i, str.GetBuffer(NULL), id);
					}
					if (var->index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
					{
						if (var->id == RETRO_DEVICE_ID_ANALOG_X)id = 18;
						if (var->id == RETRO_DEVICE_ID_ANALOG_Y)id = 19;
						input_device->bl->add(keyboard, i, str.GetBuffer(NULL), id);
					}
				}
				else if (var->device == RETRO_DEVICE_JOYPAD)
				{
					input_device->bl->add(keyboard, i, str.GetBuffer(NULL), id);
				}
				
				i++;
				++var;
			}
			Std_File_Writer_u out2;
			if (!out2.open(buffer))
			{
				input_device->save(out2);
				out2.close();
			}
		}

		
		return true;
	}
	break;

	case RETRO_ENVIRONMENT_SET_VARIABLES:
	{
		struct retro_variable *var = (struct retro_variable *)data;
		init_coresettings(var);
		return true;
	}
	break;
	case RETRO_ENVIRONMENT_GET_VARIABLE:
	{
		struct retro_variable * variable = (struct retro_variable*)data;
		variable->value = load_coresettings(variable);
		return true;
	}
	break;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;
		if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
			return false;
		return video_set_pixel_format(*fmt);
	}
	case RETRO_ENVIRONMENT_SET_HW_RENDER: {
		struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
		if (hw->context_type == RETRO_HW_CONTEXT_VULKAN)return false;
		hw->get_current_framebuffer = core_get_current_framebuffer;
		hw->get_proc_address = (retro_hw_get_proc_address_t)get_proc;
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
	input *input_device = input::GetSingleton();
	
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port != 0)return 0;
	input *input_device = input::GetSingleton();
	input_device->poll();
	if (input_device->bl != NULL)
	{
		
		for (unsigned int i = 0; i < input_device->bl->get_count(); i++) {
			{
				if (device == RETRO_DEVICE_ANALOG)
				{
					if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
					{
						int retro_id = 0;
						int16_t value = 0;
						bool isanalog = false;
						input_device->getbutton(i, value, retro_id,isanalog);
						if (value <= -0x7fff)value = -0x7fff;
						if (value >= 0x7fff)value = 0x7fff;
						if(id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 16)return value;
						if (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 17)
						{
							int16_t var = isanalog ? -value : value;
							return var;
						}
					}
				}
				else if(device == RETRO_DEVICE_JOYPAD)
				{
					int retro_id;
					int16_t value = 0;
					bool isanalog=false;
					input_device->getbutton(i, value, retro_id,isanalog);
					if (retro_id == id)return value;
				}
			}
		}
	}
	return 0;
}


void CLibretro::core_audio_sample(int16_t left, int16_t right)
{
	if (_samplesCount < SAMPLE_COUNT - 1)
	{
		_samples[_samplesCount++] = left;
		_samples[_samplesCount++] = right;
	}
}


size_t CLibretro::core_audio_sample_batch(const int16_t *data, size_t frames)
{
	if (_samplesCount < SAMPLE_COUNT - frames * 2 + 1)
	{
		memcpy(_samples + _samplesCount, data, frames * 2 * sizeof(int16_t));
		_samplesCount += frames * 2;
	}
	return frames;
}


static void core_audio_sample(int16_t left, int16_t right) {
	CLibretro* lib = CLibretro::GetSingleton();
	lib->core_audio_sample(left, right);
}


static size_t core_audio_sample_batch(const int16_t *data, size_t frames) {
	CLibretro* lib = CLibretro::GetSingleton();
	lib->core_audio_sample_batch(data, frames);
	return frames;
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

static void noop() { 
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
	if (isEmulating)isEmulating = false;
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
	if (isEmulating)isEmulating = false;
	struct retro_system_info system = {0};
	
	g_video = { 0 };
	g_video.hw.version_major = 3;
	g_video.hw.version_minor = 3;
	g_video.hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
	g_video.hw.context_reset = NULL;
	g_video.hw.context_destroy = NULL;

	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);
	
core_load(_T("cores/parallel_n64_libretro.dll"));
	filename = "sm64.z64";
	//core_load(_T("cores/snes9x_libretro.dll"));
	//filename = "smw.sfc";
	struct retro_game_info info = { filename, 0 };
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
	if (info.data)
	free((void*)info.data);

	
	thread_handle = CreateThread(NULL, 0, libretro_thread, (void*) this, 0, &thread_id);
	
	return isEmulating;
}


DWORD WINAPI CLibretro::libretro_thread(void* Param)
{
	CLibretro* This = (CLibretro*)Param;
	retro_system_av_info av = { 0 };
	g_retro.retro_get_system_av_info(&av);

	::video_configure(&av.geometry, This->emulator_hwnd);
	
	double orig_ratio = 0;
	DEVMODE lpDevMode;
	memset(&lpDevMode, 0, sizeof(DEVMODE));
	lpDevMode.dmSize = sizeof(DEVMODE);
	lpDevMode.dmDriverExtra = 0;
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode) == 0) {
		orig_ratio = (double)60 / av.timing.fps; // default value if cannot retrieve from user settings.
	}
	else
	{
		orig_ratio = (double)lpDevMode.dmDisplayFrequency / av.timing.fps;
	}
	double sampleRate = av.timing.sample_rate * orig_ratio;
	This->_samples = (int16_t*)malloc(SAMPLE_COUNT);
	memset(This->_samples, 0, SAMPLE_COUNT);
	This->_audio = new Audio();
	This->_audio->init(sampleRate);

	This->isEmulating = true;
	This->listDeltaMA.clear();
	This->frame_count = 0;
	This->fps = 0;

	This->run();

	g_retro.retro_unload_game();
	g_retro.retro_deinit();
	This->_audio->destroy();
	delete[] This->_audio;
	video_deinit();

	return 0;
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


double CLibretro::getDeltaMovingAverage(double delta)
{
	if (listDeltaMA.size() == 2048)listDeltaMA.clear();
	listDeltaMA.push_back(delta);
	double sum = 0;
	for (std::list<double>::iterator p = listDeltaMA.begin(); p != listDeltaMA.end(); ++p)
	sum += (double)*p;
	return sum / listDeltaMA.size();
}



bool CLibretro::sync_video_tick(void)
{
	static uint64_t fps_time = 0;
	bool fps_updated = false;
	uint64_t now = milliseconds_now();

//	if (frame_count++ % 120 == 0)
	{
		fps = (1000.0) / (now - fps_time);
		fps_time = now;
		fps_updated = true;
	}

	return fps_updated;
}


void CLibretro::run()
{
	while(isEmulating)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		_samplesCount = 0;
		
		while (!_samplesCount)
		{
			g_retro.retro_run();
		}
	
	
	//	sync_video_tick();
	//	retro_time_t avg = getDeltaMovingAverage(fps);
	
		/*
		int available = fifo_write_avail(_audio->_fifo);
		int total = SAMPLE_COUNT;
		int low_water_size = (unsigned)(total * 3 / 4);
		assert(total != 0);
		int half = total / 2;
		int delta_half = available - half;
		double adjust = 1.0 + 0.005 * ((double)delta_half / half);
		if (avg < 60 && (available < low_water_size)) {
			retro_system_av_info av = { 0 };
			g_retro.retro_get_system_av_info(&av);
			double sampleRate = av.timing.sample_rate * adjust;
			//double sampleRate = av.timing.sample_rate * refreshRate / av.timing.fps;
			_audio->setRate(sampleRate);
		}*/
		_audio->mix(_samples, _samplesCount / 2);
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

