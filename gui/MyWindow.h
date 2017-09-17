#ifndef MYWINDOW_H_INCLUDED
#define MYWINDOW_H_INCLUDED
#include "../stdafx.h"
#include "../CLibretro.h"
#include "DropFileTarget.h"
#include "DlgTabCtrl.h"
#include "utf8conv.h"
using namespace std;
using namespace utf8util;

class CVideoView : public CDialogImpl<CVideoView>
{
public:
	HWND m_hwndOwner; 
	enum { IDD = IDD_VIDEO };
	BEGIN_MSG_MAP(CVideoView)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
	END_MSG_MAP()

	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		return TRUE;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: Add validation code
		EndDialog(wID);
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};

class CInputView : public CDialogImpl<CInputView>
{
public:
	HWND m_hwndOwner;
	enum { IDD = IDD_INPUT };
	BEGIN_MSG_MAP(CInputView)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
	END_MSG_MAP()

	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		return TRUE;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: Add validation code
		EndDialog(wID);
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};

class CMyWindow : public CFrameWindowImpl<CMyWindow>, public CDropFileTarget<CMyWindow>,public CMessageFilter
{
public:
	DECLARE_FRAME_WND_CLASS ( _T("emu_wtl"), IDR_MAINFRAME );
	BEGIN_MSG_MAP_EX(CMyWindow)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE,OnSize)
		COMMAND_ID_HANDLER_EX(IDC_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER_EX(IDC_ABOUT, OnAbout)
		COMMAND_ID_HANDLER(ID_OPTIONS_PREFERENCES, OnOptions)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMyWindow>)
		CHAIN_MSG_MAP(CDropFileTarget<CMyWindow>)
		END_MSG_MAP()

		CLibretro *emulator;


		BOOL PreTranslateMessage(MSG* pMsg)
		{
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

		void DoFrame()
		{
			//if (emulator->running())emulator->run();
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
			emulator = CLibretro::CreateInstance(m_hWnd ) ;
			RegisterDropTarget();
			SetRedraw(FALSE);
			TIMECAPS tc; timeGetDevCaps(&tc, sizeof(TIMECAPS)); 
			UINT gwTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax); 
			timeBeginPeriod(gwTimerRes);

			CHAR szFileName[MAX_PATH];
			string ansi = ansi_from_utf16(L"smw.sfc");
			strcpy(szFileName, ansi.c_str());
			emulator->loadfile(szFileName);

			return 0;
		}

		void ProcessFile(LPCTSTR lpszPath)
		{
			CHAR szFileName[MAX_PATH];
			string ansi = ansi_from_utf16(lpszPath);
			strcpy(szFileName,ansi.c_str());
			emulator->loadfile(szFileName);
		}

		LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			TIMECAPS tc; timeGetDevCaps(&tc, sizeof(TIMECAPS));
			UINT gwTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
			timeEndPeriod(gwTimerRes);
			if (emulator)emulator->kill();
			CMessageLoop* pLoop = _Module.GetMessageLoop();
			ATLASSERT(pLoop != NULL);
			pLoop->RemoveMessageFilter(this);
			bHandled = FALSE;
			return 0;
		}

		LRESULT OnOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
		{
			//COptionsTab dlg;
			//dlg.DoModal();
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
				string ansi = ansi_from_utf16(dlg.m_szFileName);
				strcpy(szFileName,ansi.c_str());
				emulator->loadfile(szFileName);
				// do stuff
			}
			return 0;
		}

		void OnAbout(UINT uCode, int nID, HWND hwndCtrl)
		{
		
			return;
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

					gamewnd.DoFrame();
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
