#ifndef _input_h_
#define _input_h_

#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>

#include "guid_container.h"
#include "dinput.h"
#include "bind_list.h"
#include "abstract_file.h"

class Data_Reader;
class Data_Writer;

class input
{
	

	// needed by SetCooperativeLevel inside enum callback
	//HWND                    hWnd;
public:
	int list_count;
	TCHAR path[MAX_PATH];
	input();
	~input();
	unsigned                bits;
	LPDIRECTINPUT8          lpDI;
	guid_container        * guids;
	dinput                * di;
	bind_list             * bl;
	static	input* m_Instance;
	static input* CreateInstance(HINSTANCE hInstance, HWND hWnd);
	static	input* GetSingleton();

	const char* open( HINSTANCE hInstance, HWND hWnd );
	void close();

	// configuration
	const char* load( Data_Reader & );

	const char* save( Data_Writer & );

	// input_i_dinput
	void poll();
	bool getbutton(int which, int & value,int & retro_id);

	unsigned read();

	void strobe();

	int get_speed();

	void set_speed( int );

	void set_paused( bool );

	void reset();

	void set_focus( bool is_focused );

	void refocus( void * hwnd );
};

#endif