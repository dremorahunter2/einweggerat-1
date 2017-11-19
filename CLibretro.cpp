#include "stdafx.h"
#include <windows.h>
#define OUTSIDE_SPEEX
#define MAL_IMPLEMENTATION
#include "CLibretro.h"
#include "libretro.h"
#include "io/gl_render.h"
#include "gui/utf8conv.h"
#define INI_IMPLEMENTATION
#include "ini.h"
#include <algorithm>
using namespace std;
using namespace utf8util;

#define SAMPLE_COUNT 8192
#define INLINE 

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
	size_t(*retro_serialize_size)(void);
	bool(*retro_serialize)(void *data, size_t size);
	bool(*retro_unserialize)(const void *data, size_t size);
	bool(*retro_load_game)(const struct retro_game_info *game);
	//	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void(*retro_unload_game)(void);
} g_retro;

static mal_uint32 sdl_audio_callback(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
	//convert from samples to the actual number of bytes.
	int count_bytes = frameCount * pDevice->channels * mal_get_sample_size_in_bytes(mal_format_s16);
	int ret = ((Audio*)pDevice->pUserData)->fill_buffer((uint8_t*)pSamples, count_bytes) / mal_get_sample_size_in_bytes(mal_format_s16);
	return ret / pDevice->channels;
	//...and back to samples
}

mal_uint32 Audio::fill_buffer(uint8_t* out, mal_uint32 count) {
	std::lock_guard<std::mutex> lg(mutex);	
	size_t avail = fifo_read_avail(_fifo);
	size_t write_size = count > avail ? avail : count;
	buffer_full.notify_all();
	fifo_read(_fifo, out, write_size);
	memset(out + write_size, 0, count - write_size);
	return count;
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

long long microseconds_now() {
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (1000000LL * now.QuadPart) / s_frequency.QuadPart;
	}
	else {
		return GetTickCount();
	}
}
void Audio::drc()
{
		int available = fifo_write_avail(_fifo);
		int total = SAMPLE_COUNT;
		int low_water_size = (unsigned)(total * 3 / 4);
		int half = total / 2;
		int delta_half = available - half;
		double adjust = 1.0 + skew * ((double)delta_half / half);
		bool underrun = (available < low_water_size);
		if (underrun) {
			resamp_ratio = resamp_original * adjust;
		}
}

static retro_time_t frame_limit_minimum_time = 0.0;
static retro_time_t frame_limit_last_time = 0.0;

bool Audio::init(double refreshra)
{
	retro_system_av_info av = { 0 };
	g_retro.retro_get_system_av_info(&av);
	system_rate = av.timing.sample_rate;
	system_fps = av.timing.fps;
	skew = fabs(1.0f - system_fps / refreshra);
	if (skew >= 0.005)skew = 0.005;
	if (skew <= 0.005)
	{
		system_rate *= ((double)refreshra / system_fps);
	}

	client_rate = 44100;
	resamp_ratio = resamp_original = (client_rate / system_rate);

	output_float = new float[SAMPLE_COUNT * 4];
	output = new int16_t[SAMPLE_COUNT * 4];
	input_float = new float[SAMPLE_COUNT];


	resample = resampler_sinc_init(resamp_ratio);
	if (mal_context_init(NULL, 0, &context) != MAL_SUCCESS) {
		printf("Failed to initialize context.");
		return false;
	}
	mal_device_config config = mal_device_config_init_playback(mal_format_s16, 2, client_rate, sdl_audio_callback);
	config.bufferSizeInFrames = 1024;
	_fifo = fifo_new(SAMPLE_COUNT);
	if (mal_device_init(&context, mal_device_type_playback, NULL, &config, this, &device) != MAL_SUCCESS) {
		mal_context_uninit(&context);
		return false;
	}
	mal_device_start(&device);
	frame_limit_last_time = microseconds_now();
	frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f/ (av.timing.fps));

	return true;
}
void Audio::destroy()
{
	{
		mal_device_stop(&device);
		mal_context_uninit(&context);
		delete[] input_float;
		delete[] output;
		delete[] output_float;
		resampler_sinc_free(resample);
	}
}
void Audio::reset()
{

}


#define MAX(x,y) ((x)>(y)) ? (x) : (y)
#define MIN(x,y) ((x)<(y)) ? (x) : (y)

void Audio::mix(const int16_t* samples, size_t frames)
{
	uint32_t in_len = frames * 2;

	drc();

	const float div = (1.0f / 32768.0f);
	for (int i = 0; i < in_len; i++) {
		input_float[i] = div * (float)samples[i];
	}

	struct resampler_data src_data = { 0 };
	src_data.input_frames = frames;
	src_data.ratio = resamp_ratio;
	src_data.data_in = input_float;
	src_data.data_out = output_float;
	resampler_sinc_process(resample, &src_data);
	int out_len = src_data.output_frames * 2 * sizeof(int16_t);

	const float mul = (32768.0f);
	for (int i = 0; i < out_len; i++) {
		int32_t tmp = (int32_t)(mul * output_float[i]);
		tmp = MAX(tmp, -32768); // CLIP < 32768
		tmp = MIN(tmp, 32767);  // CLIP > 32767
		output[i] = tmp;
	}

	std::unique_lock<std::mutex> l(lock);
	buffer_full.wait(l, [this, out_len]() {return out_len < fifo_write_avail(_fifo); });

	fifo_write(_fifo, output, out_len);

	retro_time_t to_sleep_ms = ((frame_limit_last_time + frame_limit_minimum_time)- microseconds_now()) / 1000;

	if (to_sleep_ms > 0)
	{
		float sleep_ms = (unsigned)to_sleep_ms;
		/* Combat jitter a bit. */
		frame_limit_last_time += frame_limit_minimum_time;
		Sleep(sleep_ms);
		return;
	}

	frame_limit_last_time = microseconds_now();
}







static void core_unload() {
	if (g_retro.initialized)
	g_retro.retro_deinit();
	if (g_retro.handle)
	{
		FreeLibrary(g_retro.handle);
		g_retro.handle = NULL;
	}
	
}

enum COLOR
{
	// Text foreground colors
	// Standard text colors
	GRAY_TEXT = 8, BLUE_TEXT, GREEN_TEXT,
	TEAL_TEXT, RED_TEXT, PINK_TEXT,
	YELLOW_TEXT, WHITE_TEXT,
	// Faded text colors
	BLACK_TEXT = 0, BLUE_FADE_TEXT, GREEN_FADE_TEXT,
	TEAL_FADE_TEXT, RED_FADE_TEXT, PINK_FADE_TEXT,
	YELLOW_FADE_TEXT, WHITE_FADE_TEXT,
	// Standard text background color
	GRAY_BACKGROUND = GRAY_TEXT << 4, BLUE_BACKGROUND = BLUE_TEXT << 4,
	GREEN_BACKGROUND = GREEN_TEXT << 4, TEAL_BACKGROUND = TEAL_TEXT << 4,
	RED_BACKGROUND = RED_TEXT << 4, PINK_BACKGROUND = PINK_TEXT << 4,
	YELLOW_BACKGROUND = YELLOW_TEXT << 4, WHITE_BACKGROUND = WHITE_TEXT << 4,
	// Faded text background color
	BLACK_BACKGROUND = BLACK_TEXT << 4, BLUE_FADE_BACKGROUND = BLUE_FADE_TEXT << 4,
	GREEN_FADE_BACKGROUND = GREEN_FADE_TEXT << 4, TEAL_FADE_BACKGROUND = TEAL_FADE_TEXT << 4,
	RED_FADE_BACKGROUND = RED_FADE_TEXT << 4, PINK_FADE_BACKGROUND = PINK_FADE_TEXT << 4,
	YELLOW_FADE_BACKGROUND = YELLOW_FADE_TEXT << 4, WHITE_FADE_BACKGROUND = WHITE_FADE_TEXT << 4
};

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
#pragma optimize( "", off )  
void init_coresettings(retro_variable *var)
{
	CLibretro * retro = CLibretro::GetSingleton();
	FILE *fp = NULL;
	fp = _wfopen(retro->corevar_path, L"r");
	
	if (!fp)
	{
		//create a new file with defaults
		ini_t* ini = ini_create(NULL);
		while (var->key && var->value)
		{
			CLibretro::core_vars vars_struct;
			strcpy(vars_struct.name, var->key);
		   
			
			char descript[50] = { 0 };
			char *e = strchr((char*)var->value, ';');
			strncpy(descript, var->value, (int)(e - (char*)var->value));
			strcpy(vars_struct.description,descript);

			char * pch = strstr((char*)var->value, (char*)"; ");
			pch += strlen("; ");
			int vars = 0;
			strcpy(vars_struct.usevars, pch);
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
					strcpy(vars_struct.var,val);
					ini_property_add(ini, INI_GLOBAL_SECTION, var->key, strlen(var->key), val, strlen(val));
				}
				pch += str2 - pch++;
				vars++;
				
			}
			retro->variables.push_back(vars_struct);
			++var;
		}
		int size = ini_save(ini, NULL, 0); // Find the size needed
		char* data = (char*)malloc(size);
		size = ini_save(ini, data, size); // Actually save the file
		ini_destroy(ini);
		fp = _wfopen(retro->corevar_path, L"w");
		fwrite(data, 1, size, fp);
		fclose(fp);
		free(data);
	}
	else
	{	
		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char* data = (char*)malloc(size + 1);
		fread(data, 1, size, fp);
		data[size] = '\0';
		fclose(fp);
		ini_t* ini = ini_load(data, NULL);
		free(data);
		while (var->key && var->value)
		{
			CLibretro::core_vars vars_struct;
			strcpy(vars_struct.name, var->key);
			char descript[50] = { 0 };
			char *e = strchr((char*)var->value, ';');
			strncpy(descript, var->value, (int)(e - (char*)var->value));
			strcpy(vars_struct.description, descript);
			char * pch = strstr((char*)var->value, (char*)"; ");
			pch += strlen("; ");
			int vars = 0;
			strcpy(vars_struct.usevars, pch);
			int second_index = ini_find_property(ini, INI_GLOBAL_SECTION, (char*)var->key, strlen(var->key));
			const char* variable_val = ini_property_value(ini, INI_GLOBAL_SECTION, second_index);
			strcpy(vars_struct.var, variable_val);
			retro->variables.push_back(vars_struct);
			++var;
		}
		ini_destroy(ini);
	}
}

const char* load_coresettings(retro_variable *var)
{
	CLibretro *retro = CLibretro::GetSingleton();
	for (int i = 0; i < retro->variables.size(); i++)
	{
		if (strcmp(retro->variables[i].name, var->key) == 0)
		{
			return retro->variables[i].var;
		}
	}
	return NULL;
}
#pragma optimize( "", on )  



bool core_environment(unsigned cmd, void *data) {
	bool *bval;
	CLibretro * retro = CLibretro::GetSingleton();
	input *input_device = input::GetSingleton();
	switch (cmd) {
	case RETRO_ENVIRONMENT_SET_MESSAGE: {
		struct retro_message *cb = (struct retro_message *)data;
		return true;
	}
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
		static char *sys_path = NULL;
		if (!sys_path)
		{
			TCHAR sys_filename[MAX_PATH] = { 0 };
			GetCurrentDirectory(MAX_PATH, sys_filename);
			PathAppend(sys_filename, L"system");
			string ansi = utf8_from_utf16(sys_filename);
			sys_path = strdup(ansi.c_str());
		}
		char **ppDir = (char**)data;
		*ppDir = sys_path;
		return true;
	}
	break;

	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: // 31
	{
		
		
		char variable_val2[50] = { 0 };
		Std_File_Reader_u out;
		lstrcpy(input_device->path, retro->inputcfg_path);
		if (!out.open(retro->inputcfg_path))
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
			if (!out2.open(retro->inputcfg_path))
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
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
	{
	*(bool*)data = retro->variables_changed;
	retro->variables_changed = false;
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
	input_device->poll();
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port != 0)return 0;
	input *input_device = input::GetSingleton();
	if (input_device && input_device->bl != NULL)
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
						if (value <= -0x8000)value = -0x7fff;
						if (value >= 0x8000)value = 0x7fff;
						if ((id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 16) || (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 17))
						{
							return isanalog ? -value : value;
						}
					}
					else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
					{
						int retro_id = 0;
						int16_t value = 0;
						bool isanalog = false;
						input_device->getbutton(i, value, retro_id, isanalog);
						if (value <= -0x8000)value = -0x7fff;
						if (value >= 0x8000)value = 0x7fff;
						if ((id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 18) || (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 19))
						{
							return isanalog ? -value : value;
						}
					}
				}
				else if (device == RETRO_DEVICE_JOYPAD)
				{
					int retro_id;
					int16_t value = 0;
					bool isanalog = false;
					input_device->getbutton(i, value, retro_id, isanalog);
					value = abs(value);
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

bool CLibretro::savestate(TCHAR* filename, bool save)
{
	if (isEmulating)
	{
		paused = true;
		size_t size = g_retro.retro_serialize_size();
		if (save)
		{
			FILE *Input = _wfopen(filename, L"wb");
			if (!Input) return(NULL);
			// Get the filesize
			BYTE *Memory = (BYTE *)malloc(size);
			g_retro.retro_serialize(Memory, size);
			fwrite(Memory, 1, size, Input);
			fclose(Input);
			Input = NULL;
			paused = false;
			return true;

		}
		else
		{
			FILE *Input = _wfopen(filename, L"rb");
			fseek(Input, 0, SEEK_END);
			int Size = ftell(Input);
			fseek(Input, 0, SEEK_SET);
			BYTE *Memory = (BYTE *)malloc(Size);
			fread(Memory, 1, Size, Input);
			g_retro.retro_unserialize(Memory, size);
			if (Input) fclose(Input);
			Input = NULL;
			paused = false;
			return true;
		}
	}
	return false;
}

void CLibretro::reset()
{
	if(isEmulating)g_retro.retro_reset();
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

bool CLibretro::core_load(TCHAR *sofile,bool gamespecificoptions, TCHAR* filename,TCHAR* core_filename) {
	
	memset(&g_retro, 0, sizeof(g_retro));
	g_retro.handle = LoadLibrary(sofile);
	if (!g_retro.handle)return false;

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
	load_retro_sym(retro_serialize);
	load_retro_sym(retro_unserialize);
	load_retro_sym(retro_serialize_size);
	void(*set_environment)(retro_environment_t) = NULL;
	void(*set_video_refresh)(retro_video_refresh_t) = NULL;
	void(*set_input_poll)(retro_input_poll_t) = NULL;
	void(*set_input_state)(retro_input_state_t) = NULL;
	void(*set_audio_sample)(retro_audio_sample_t) = NULL;
	void(*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
#define load_sym(V,name) if (!(*(void**)(&V)=(void*)libload(#name)))
	load_sym(set_environment, retro_set_environment);
	load_sym(set_video_refresh, retro_set_video_refresh);
	load_sym(set_input_poll, retro_set_input_poll);
	load_sym(set_input_state, retro_set_input_state);
	load_sym(set_audio_sample, retro_set_audio_sample);
	load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);


	TCHAR core_filename2[MAX_PATH] = { 0 };
	GetModuleFileNameW(g_retro.handle, core_filename2, sizeof(core_filename2));


	if (gamespecificoptions)
	{
		TCHAR filez[MAX_PATH] = { 0 };
		lstrcpy(filez, filename);
		memset(inputcfg_path, 0, MAX_PATH);
		memset(corevar_path, 0, MAX_PATH);
		PathRemoveExtension(core_filename2);
		PathStripPath(filez);
		PathRemoveExtension(filez);
		lstrcpy(inputcfg_path, core_filename2);
		lstrcpy(corevar_path, core_filename2);
		PathAppend(inputcfg_path, filez);
		PathAppend(corevar_path, filez);
		lstrcat(inputcfg_path, L"_input.cfg");
		lstrcat(corevar_path, L".ini");

	}
	else
	{
		memset(inputcfg_path, 0, MAX_PATH);
		memset(corevar_path, 0, MAX_PATH);
		PathRemoveExtension(core_filename2);
		lstrcat(inputcfg_path, core_filename2);
		lstrcat(corevar_path, core_filename2);
		lstrcat(inputcfg_path, L"_input.cfg");
		lstrcat(corevar_path, L".ini");
	}

	//set libretro func pointers
	set_environment(core_environment);
	set_video_refresh(core_video_refresh);
	set_input_poll(core_input_poll);
	set_input_state(core_input_state);
	set_audio_sample(::core_audio_sample);
	set_audio_sample_batch(::core_audio_sample_batch);
	g_retro.retro_init();
	g_retro.initialized = true;
	return true;
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

#include <sys/stat.h>

#include <dwmapi.h>
#pragma comment (lib,"dwmapi.lib")
bool CLibretro::loadfile(TCHAR* filename, TCHAR* core_filename,bool gamespecificoptions)
{
	if (isEmulating)isEmulating = false;
	struct retro_system_info system = {0};	
	g_video = { 0 };
	g_video.hw.version_major = 3;
	g_video.hw.version_minor = 3;
	g_video.hw.context_type = RETRO_HW_CONTEXT_NONE;
	g_video.hw.context_reset = NULL;
	g_video.hw.context_destroy = NULL;
	variables_changed = false;

	if (!core_load(core_filename,gamespecificoptions,filename,core_filename))
	{
		printf("FAILED TO LOAD CORE!!!!!!!!!!!!!!!!!!");
		return false;
	}
	
	CHAR szFileName[MAX_PATH] = { 0 };
	string ansi = utf8_from_utf16(filename);
	strcpy(szFileName, ansi.c_str());
	struct retro_game_info info = {0};
	struct stat st;
	stat(szFileName, &st);
	info.path = szFileName;
	info.data = NULL;
	info.size = st.st_size;
	info.meta = NULL;

	g_retro.retro_get_system_info(&system);
	if (!system.need_fullpath) {
		FILE *Input = _wfopen(filename, L"rb");
		if (!Input)
		{
			printf("FAILED TO LOAD ROMz!!!!!!!!!!!!!!!!!!");
			return false;
		}
		
		// Get the filesize
		fseek(Input, 0, SEEK_END);
		int Size = ftell(Input);
		fseek(Input, 0, SEEK_SET);
		BYTE *Memory = (BYTE *)malloc(Size);
		if (!Memory) return(NULL);
		if (fread(Memory, 1, Size, Input) != (size_t)Size) return(NULL);
		if (Input) fclose(Input);
		Input = NULL;
		info.data = malloc(info.size);
		memcpy((BYTE*)info.data, Memory, info.size);
		free(Memory);
	}
	if (!g_retro.retro_load_game(&info))
	{
		printf("FAILED TO LOAD ROM!!!!!!!!!!!!!!!!!!");
		return false;
	}
	if (info.data)free((void*)info.data);

	retro_system_av_info av = { 0 };
	g_retro.retro_get_system_av_info(&av);

	::video_configure(&av.geometry, emulator_hwnd);

	_samples = (int16_t*)malloc(SAMPLE_COUNT);
	memset(_samples, 0, SAMPLE_COUNT);

	DWM_TIMING_INFO timing_info;
	timing_info.cbSize = sizeof(timing_info);
	DwmGetCompositionTimingInfo(NULL, &timing_info);
	double refreshr = (timing_info.qpcRefreshPeriod) / 1000;
	_audio.init(refreshr);
	frame_count = 0;
	paused = false;
	isEmulating = true;
	lastTime = milliseconds_now()/1000;
    nbFrames = 0;

	return true;
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


void CLibretro::run()
{
	if(isEmulating)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		_samplesCount = 0;
		if (!paused)
		{
			while(!_samplesCount)
			{
				g_retro.retro_run();
			}
		}
	    _audio.mix(_samples, _samplesCount/2);

			// Measure speed
			double currentTime = milliseconds_now()/1000;
			nbFrames++;
			if (currentTime - lastTime >= 0.5) { // If last prinf() was more than 1 sec ago
												 // printf and reset timer
				TCHAR buffer[100] = { 0 };
				int len = swprintf(buffer, 100, L"einweggerat - 64bit: %2f ms/frame\n, %d FPS", 1000.0 / double(nbFrames),nbFrames);
				SetWindowText(emulator_hwnd, buffer);
				nbFrames = 0;
				lastTime += 1.0;
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
	_audio.destroy();
	video_deinit();
	g_retro.retro_unload_game();
	g_retro.retro_deinit();

	
	//WindowsAudio::Close();
}

