// MyFirstWTLWindow.cpp : Defines the entry point for the application.
//

#include "../stdafx.h"
#include "resource.h"
#include "MyWindow.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "../cmdline.h"
#include <iostream>
#include <string>
#include <sstream>
using namespace std;
CAppModule _Module;

static const WORD MAX_CONSOLE_LINES = 1000;

void RedirectIOToConsole()
{
	long lStdHandle;
	FILE *fp;
	int hConHandle;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	// set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),
		coninfo.dwSize);
	// redirect unbuffered STDOUT to the console
	lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen(hConHandle, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);
}



LPSTR* CommandLineToArgvA(LPSTR lpCmdLine, INT *pNumArgs)
{
	int retval;
	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
	if (!SUCCEEDED(retval))
		return NULL;

	LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
	if (lpWideCharStr == NULL)
		return NULL;

	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
	if (!SUCCEEDED(retval))
	{
		free(lpWideCharStr);
		return NULL;
	}

	int numArgs;
	LPWSTR* args;
	args = CommandLineToArgvW(lpWideCharStr, &numArgs);
	free(lpWideCharStr);
	if (args == NULL)
		return NULL;

	int storage = numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(args);
			return NULL;
		}

		storage += retval;
	}

	LPSTR* result = (LPSTR*)LocalAlloc(LMEM_FIXED, storage);
	if (result == NULL)
	{
		LocalFree(args);
		return NULL;
	}

	int bufLen = storage - numArgs * sizeof(LPSTR);
	LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		assert(bufLen > 0);
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(result);
			LocalFree(args);
			return NULL;
		}

		result[i] = buffer;
		buffer += retval;
		bufLen -= retval;
	}

	LocalFree(args);

	*pNumArgs = numArgs;
	return result;
}


int Run(LPTSTR cmdline = NULL, int nCmdShow = SW_SHOWDEFAULT)
{



	CEmuMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);
	CMyWindow dlgMain;
	// Create the main window
	if ( NULL == dlgMain.CreateEx() )
		return 1;       // uh oh, window creation failed
	// Show the window
	LONG style = dlgMain.GetWindowLong(GWL_STYLE);
	style &= ~WS_POPUP;
	dlgMain.SetWindowLong(GWL_STYLE,style);
	dlgMain.SetWindowPos(0,100,100,640,480,SWP_NOZORDER);
	dlgMain.CenterWindow();
	dlgMain.SetIcon(LoadIcon(_Module.GetResourceInstance(),MAKEINTRESOURCE(IDI_ICON1)));
#ifdef _WIN64
	dlgMain.SetWindowText(L"einweggerät - 64bit");
#else
	dlgMain.SetWindowText(L"einweggerät - 32bit");
#endif
	CMenu menu;
	menu.Attach(LoadMenu( _Module.GetResourceInstance(), MAKEINTRESOURCE(MENU_MAINFRAME)));
	dlgMain.SetMenu(menu);



	int argc = 1;
	char** cmdargptr = CommandLineToArgvA(GetCommandLineA(), &argc);

	if (argc < 2)
	{
		if (!AttachConsole(ATTACH_PARENT_PROCESS))
		{
			AllocConsole();
			AttachConsole(GetCurrentProcessId());
			freopen("CONOUT$", "w", stdout);
			cmdline::parser a;
			a.add<string>("core_name", 'c', "core filename", true, "");
			a.add<string>("rom_name", 'r', "rom filename", true, "");
			a.add("pergame", 'g', "per-game configuration");
			a.parse_check(argc, cmdargptr);
		    BOOL set=false;
			int nRet = theLoop.Run(dlgMain);
			dlgMain.DestroyWindow();
			_Module.RemoveMessageLoop();
			ExitProcess(0);
			return nRet;
		}
		else
		{
			freopen("CONOUT$", "w", stdout);
			cmdline::parser a;
			a.add<string>("core_name", 'c', "core filename", true, "");
			a.add<string>("rom_name", 'r', "rom filename", true, "");
			a.add("pergame", 'g', "per-game configuration");
			a.parse_check(argc, cmdargptr);
			printf("Press any key to continue....\n");
			dlgMain.DestroyWindow();
			_Module.RemoveMessageLoop();
			ExitProcess(0);
			return 0;
		}

	}

	if (!AttachConsole(ATTACH_PARENT_PROCESS))
	{
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		freopen("CONOUT$", "w", stdout);
	}
	else
	{
		freopen("CONOUT$", "w", stdout);
	}
	cmdline::parser a;
	a.add<string>("core_name", 'c', "core filename", true, "");
	a.add<string>("rom_name", 'r', "rom filename", true, "");
	a.add("pergame", 'g', "per-game configuration");
	a.parse_check(argc, cmdargptr);
	dlgMain.ShowWindow(nCmdShow);
	wstring rom = s2ws(a.get<string>("rom_name"));
	wstring core = s2ws(a.get<string>("core_name"));
	bool percore = a.exist("pergame");
	dlgMain.start((TCHAR*)rom.c_str(), (TCHAR*)core.c_str(), percore);
	int nRet = theLoop.Run(dlgMain);
	_Module.RemoveMessageLoop();
	ExitProcess(0);
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(NULL);
	// If you are running on NT 4.0 or higher you can use the following call instead to 
	// make the EXE free threaded. This means that calls come in on a random RPC thread.
	//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));
	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);
	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls
	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));
	int nRet = Run(lpstrCmdLine, nCmdShow);
	_Module.Term();
	::CoUninitialize();
	return nRet;
}