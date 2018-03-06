#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>


#include "io/input.h"
#include "io/audio.h"





namespace std
{
	typedef wstring        tstring;
	typedef wistringstream tistringstream;
	typedef wostringstream tostringstream;
}



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
	TCHAR sys_filename[MAX_PATH];
	TCHAR sav_filename[MAX_PATH];
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
	bool savesram(TCHAR* filename, bool save = false);
	void kill();
	BOOL isEmulating;
	void core_audio_sample(int16_t left, int16_t right);
	size_t core_audio_sample_batch(const int16_t *data, size_t frames);
	Audio  _audio;

	double lastTime;
    int nbFrames;
};


#endif CEMULATOR_H