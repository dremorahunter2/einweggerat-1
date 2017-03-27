#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>


#ifdef __cplusplus
extern "C" {
#endif

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
};

#ifdef __cplusplus
}
#endif

#endif CEMULATOR_H