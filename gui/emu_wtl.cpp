// MyFirstWTLWindow.cpp : Defines the entry point for the application.
//

#include "../stdafx.h"
#include "resource.h"
#include "MyWindow.h"

CAppModule _Module;
int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
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
	dlgMain.SetWindowText(L"libretro loader - 64bit");
#else
	dlgMain.SetWindowText(L"libretro loader - 32bit");
#endif
	CMenu menu;
	menu.Attach(LoadMenu( _Module.GetResourceInstance(), MAKEINTRESOURCE(MENU_MAINFRAME)));
	dlgMain.SetMenu(menu);
	dlgMain.ShowWindow ( nCmdShow );
	
	
	int nRet = theLoop.Run(dlgMain);
	_Module.RemoveMessageLoop();
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