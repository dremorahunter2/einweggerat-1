#ifndef MYWINDOW_H_INCLUDED
#define MYWINDOW_H_INCLUDED
#include "../stdafx.h"
#include "../CLibretro.h"
#include "DropFileTarget.h"
#include "DlgTabCtrl.h"
#include "utf8conv.h"
#include <cmath>
#include "PropertyGrid.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include "ini.h"
#include "libretro.h"
#include <shlwapi.h>
#include "cmdline.h"
#include <dwmapi.h>
#pragma comment (lib,"dwmapi.lib")

using namespace std;
using namespace utf8util;

#include "InputConfig.h"
#include "CoreVariablesConfig.h"
#pragma comment(lib, "shlwapi.lib")



#include "../gitver.h"
class CAboutDlg : public CDialogImpl<CAboutDlg>
{
	CHyperLink website;
	CStatic version_number;
	CStatic builddate;
	CEdit greets;
public:
	enum { IDD = IDD_ABOUT };
	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
		COMMAND_ID_HANDLER(IDOK,OnCancel)
	END_MSG_MAP()

	CAboutDlg() { }

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}


	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		//CString verinf0 = SVN_REVISION;
		CString date = "Built on " __DATE__ " at " __TIME__ " (GMT+10)";
		CString verinfo = "einweggerät public build " GIT_VERSION;

		CString greetz;

		CenterWindow();

		greets = GetDlgItem(IDC_CREDITS);
		builddate = GetDlgItem(IDC_BUILDDATE);
		builddate.SetWindowText(date);
		version_number = GetDlgItem(IDC_APPVER);
		version_number.SetWindowText(verinfo);

		greetz += "einweggerät\r\n";
		greetz += "-----------\r\n";
		greetz += "Keys: \r\n";
		greetz += "F1 : Load Savestate\r\n";
		greetz += "F2 : Save Savestate\r\n";
		greetz += "F3 : Reset\r\n";
		greetz += "-----------\r\n";
		greetz += "Commandline variables:\r\n";
		greetz += "-r (game filename)\r\n";
		greetz += "-c (core filename)\r\n";
		greetz += "-q : Per-game configuration\r\n";
		greetz += "\n";
		greetz += "Example: einweggerat.exe -r somerom.sfc  -c snes9x_libretro.dll\r\n";
		greetz += "-----------\r\n";
		greetz += "Greetz:\r\n";
		greetz += "Higor Eurípedes\r\n";
		greetz += "Andre Leiradella\r\n";
		greetz += "Daniel De Matteis\r\n";
		greetz += "Andrés Suárez\r\n";
		greetz += "Brad Parker\r\n";
		greetz += "Chris Snowhill\r\n";
		greetz += "Hunter Kaller\r\n";
		greetz += "Alfred Agrell\r\n";
		
		greets.SetWindowText(greetz);
		website.SubclassWindow(GetDlgItem(IDC_LINK));
		website.SetHyperLink(_T("http://mudlord.info"));
		return TRUE;
	}
};


class CMyWindow : public CFrameWindowImpl<CMyWindow>, public CDropFileTarget<CMyWindow>,public CMessageFilter
{
public:
	DECLARE_FRAME_WND_CLASS ( _T("emu_wtl"), NULL);
	BEGIN_MSG_MAP_EX(CMyWindow)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE,OnSize)
		COMMAND_ID_HANDLER(ID_PREFERENCES_INPUTCONFIG, OnInput)
		COMMAND_ID_HANDLER(ID_PREFERENCES_COREVARIABLES, OnVariables)
		COMMAND_ID_HANDLER_EX(IDC_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_ABOUT, OnAbout)
		COMMAND_ID_HANDLER(ID_LOADSTATEFILE,OnLoadState)
		COMMAND_ID_HANDLER(ID_SAVESTATEFILE, OnSaveState)
		COMMAND_ID_HANDLER(ID_RESET, OnReset)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMyWindow>)
		CHAIN_MSG_MAP(CDropFileTarget<CMyWindow>)
		END_MSG_MAP()

		CLibretro *emulator;
		input*    input_device;
		HACCEL    m_haccelerator;

		LRESULT OnLoadState(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			CHAR szFileName[MAX_PATH];
			LPCTSTR sFiles =
				L"Savestates (*.state)\0*.state\0"
				L"All Files (*.*)\0*.*\0\0";
			CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);
			if (dlg.DoModal() == IDOK)
			{
				emulator->savestate(dlg.m_szFileName);
				// do stuff
			}

			return 0;
		}

		LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			emulator->reset();

			return 0;
		}

		LRESULT OnSaveState(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			CHAR szFileName[MAX_PATH];
			LPCTSTR sFiles =
				L"Savestates (*.state)\0*.state\0"
				L"All Files (*.*)\0*.*\0\0";
			CFileDialog dlg(FALSE, L"*.state", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);
			if (dlg.DoModal() == IDOK)
			{
				emulator->savestate(dlg.m_szFileName,true);
				// do stuff
			}
			return 0;
		}

		BOOL PreTranslateMessage(MSG* pMsg)
		{
			if (m_haccelerator != NULL)
			{
				if (::TranslateAccelerator(m_hWnd, m_haccelerator, pMsg))
					return TRUE;
			}

			return CWindow::IsDialogMessage(pMsg);
		}

		BOOL IsRunning()
		{
			if (emulator)
			{
				emulator->splash();
				return emulator->running();
			}
			return FALSE;
		}

		LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (!emulator->isEmulating)
			{
				PAINTSTRUCT ps;
				HDC pDC = BeginPaint(&ps);
				RECT rect;
				GetClientRect(&rect);
				// I don't really want it black but this
				// will do for testing the concept:
				HBRUSH hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
				::FillRect(pDC, &rect, hBrush);
				::ValidateRect(m_hWnd, &rect);
				EndPaint(&ps);
			}
			return 0;
		}


		LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			CMessageLoop* pLoop = _Module.GetMessageLoop();	
			ATLASSERT(pLoop != NULL);
			bHandled = FALSE;
			input_device = input::CreateInstance(GetModuleHandle(NULL), m_hWnd);
			emulator = CLibretro::CreateInstance(m_hWnd ) ;
			m_haccelerator = AtlLoadAccelerators(IDR_ACCELERATOR1);
			pLoop->AddMessageFilter(this);
			RegisterDropTarget();
			SetRedraw(FALSE);
			DwmEnableMMCSS(TRUE);
			return 0;
		}


		void start(TCHAR* rom_filename,TCHAR* core_filename, bool specifics)
		{
			TCHAR core_filename2[MAX_PATH] = { 0 };
			GetCurrentDirectory(MAX_PATH, core_filename2);
			PathAppend(core_filename2, L"cores");
			bool set = SetDllDirectory(core_filename2);
			if (lstrcmp(core_filename, L"") == 0 || lstrcmp(rom_filename, L"") == 0)
			{
				CAboutDlg dlg;
				dlg.DoModal();
				DestroyWindow();
				return;
			}
			if (!emulator->loadfile(rom_filename, core_filename, specifics))
				DestroyWindow();
		}

		void ProcessFile(LPCTSTR lpszPath)
		{
			/**
			CHAR szFileName[MAX_PATH];
			string ansi = ansi_from_utf16(lpszPath);
			strcpy(szFileName,ansi.c_str());
			emulator->loadfile(szFileName);*/
		}

		LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			DwmEnableMMCSS(FALSE);
			if (emulator)emulator->kill();
			if (input_device)input_device->close();
			CMessageLoop* pLoop = _Module.GetMessageLoop();
			ATLASSERT(pLoop != NULL);
			pLoop->RemoveMessageFilter(this);
			bHandled = FALSE;
			return 0;
		}

		LRESULT OnInput(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
		{
			CInputView dlg;
			dlg.DoModal();

			return 0;
		}

		LRESULT OnVariables(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			CVariablesView dlg;
			dlg.DoModal();

			return 0;
		}

		void OnFileExit(UINT uCode, int nID, HWND hwndCtrl)
		{
			DestroyWindow();
			return;
		}

		LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
		{
			CHAR szFileName[MAX_PATH];
			LPCTSTR sFiles =
				L"GB ROMs (*.dmg,*.gb)\0*.dmg;*.gb\0"
				L"All Files (*.*)\0*.*\0\0";
			CFileDialog dlg( TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);
			if (dlg.DoModal() == IDOK)
			{
			//	string ansi = ansi_from_utf16(dlg.m_szFileName);
			//	strcpy(szFileName,ansi.c_str());
			///	emulator->loadfile(szFileName);
				// do stuff
			}
			return 0;
		}

		LRESULT OnAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			CAboutDlg dlg;
			dlg.DoModal();

			return 0;
		}
};

class CEmuMessageLoop : public CMessageLoop
{
public:
	int Run( CMyWindow & gamewnd )
	{
		BOOL bDoIdle = TRUE;
		int nIdleCount = 0;
		BOOL bRet = FALSE;

		for(;;)
		{

			if ( gamewnd.IsRunning() ) {
				while ( gamewnd.IsRunning() && !::PeekMessage(&m_msg, NULL, 0, 0, PM_NOREMOVE) )
				{
					while(bDoIdle && !::PeekMessage(&m_msg, NULL, 0, 0, PM_NOREMOVE))
					{
						if(!OnIdle(nIdleCount++))
							bDoIdle = FALSE;
					}
				}


			}
			else
			{
				while(bDoIdle && !::PeekMessage(&m_msg, NULL, 0, 0, PM_NOREMOVE))
				{
					if(!OnIdle(nIdleCount++))
						bDoIdle = FALSE;
				}
			}

			bRet = ::GetMessage(&m_msg, NULL, 0, 0);

			if(bRet == -1)
			{
				ATLTRACE2(atlTraceUI, 0, _T("::GetMessage returned -1 (error)\n"));
				continue;   // error, don't process
			}
			else if(!bRet)
			{
				ATLTRACE2(atlTraceUI, 0, _T("CMessageLoop::Run - exiting\n"));
				break;   // WM_QUIT, exit message loop
			}

			if(!PreTranslateMessage(&m_msg))
			{
				::TranslateMessage(&m_msg);
				::DispatchMessage(&m_msg);
			}

			if(IsIdleMessage(&m_msg))
			{
				bDoIdle = TRUE;
				nIdleCount = 0;
			}
		}

		return (int)m_msg.wParam;
	}
};


#endif  // ndef MYWINDOW_H_INCLUDED
