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

using namespace std;
using namespace utf8util;

#define PGS_EX_SINGLECLICKEDIT   0x00000001
#define PGS_EX_NOGRID            0x00000002
#define PGS_EX_TABNAVIGATION     0x00000004
#define PGS_EX_NOSHEETNAVIGATION 0x00000008
#define PGS_EX_FULLROWSELECT     0x00000010
#define PGS_EX_INVERTSELECTION   0x00000020
#define PGS_EX_ADDITEMATEND      0x00000040

namespace std
{
	typedef wstring        tstring;
	typedef wistringstream tistringstream;
	typedef wostringstream tostringstream;
}

class CVariablesView : public CDialogImpl<CVariablesView>
{
public:
	HWND m_hwndOwner; 
	enum { IDD = IDD_VARIABLES };
	BEGIN_MSG_MAP(CVariablesView)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
		NOTIFY_CODE_HANDLER(PIN_SELCHANGED, OnSelChanged);
	NOTIFY_CODE_HANDLER(PIN_ITEMCHANGED, OnItemChanged);
	NOTIFY_CODE_HANDLER(PIN_ADDITEM, OnAddItem);
	REFLECT_NOTIFICATIONS()
	END_MSG_MAP()
	CPropertyGridCtrl m_grid;


	LRESULT OnAddItem(int idCtrl, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	{
		ATLTRACE(_T("OnAddItem - Ctrl: %d\n"), idCtrl); idCtrl;

		int i = m_grid.InsertItem(-1, PropCreateReadOnlyItem(_T(""), _T("Dolly")));
		m_grid.SetSubItem(i, 1, PropCreateSimple(_T(""), true));
		m_grid.SetSubItem(i, 2, PropCreateCheckButton(_T(""), false));
		m_grid.SetSubItem(i, 3, PropCreateSimple(_T(""), _T("")));
		m_grid.SelectItem(i);
		return 0;
	}

	LRESULT OnSelChanged(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPNMPROPERTYITEM pnpi = (LPNMPROPERTYITEM)pnmh;
		if (pnpi->prop == NULL) return 0;
		TCHAR szValue[100] = { 0 };
		pnpi->prop->GetDisplayValue(szValue, sizeof(szValue) / sizeof(TCHAR));
		CComVariant vValue;
		pnpi->prop->GetValue(&vValue);
		vValue.ChangeType(VT_BSTR);
		ATLTRACE(_T("OnSelChanged - Ctrl: %d, Name: '%s', DispValue: '%s', Value: '%ls'\n"),
			idCtrl, pnpi->prop->GetName(), szValue, vValue.bstrVal); idCtrl;
		return 0;
	}

	LRESULT OnItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPNMPROPERTYITEM pnpi = (LPNMPROPERTYITEM)pnmh;
		TCHAR szValue[100] = { 0 };
		pnpi->prop->GetDisplayValue(szValue, sizeof(szValue) / sizeof(TCHAR));
		CComVariant vValue;
		pnpi->prop->GetValue(&vValue);
		vValue.ChangeType(VT_BSTR);
		ATLTRACE(_T("OnItemChanged - Ctrl: %d, Name: '%s', DispValue: '%s', Value: '%ls'\n"),
			idCtrl, pnpi->prop->GetName(), szValue, vValue.bstrVal); idCtrl;
		return 0;
	}


	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{




		LPCTSTR boolvar[] = { _T("enabled"), _T("disabled"), NULL };
		m_grid.SubclassWindow(GetDlgItem(IDC_LIST_VARIABLES));

		m_grid.InsertColumn(0, _T("Variable"), LVCFMT_LEFT, 200, 0);
		m_grid.InsertColumn(1, _T("Value"), LVCFMT_LEFT, 80, 0);

		m_grid.SetExtendedGridStyle(PGS_EX_SINGLECLICKEDIT);
		LPCTSTR pList[] = { _T("White"), _T("Brown"), _T("Pink"), _T("Yellow"), NULL };
		m_grid.InsertItem(0, PropCreateReadOnlyItem(_T(""), _T("Raymond")));
		m_grid.SetSubItem(0,1,PropCreateList(_T("Skin"), pList));


		HPROPERTY hBeth = PropCreateReadOnlyItem(_T(""), _T("Beth"));
		m_grid.InsertItem(1, hBeth);
		m_grid.SetSubItem(1, 1, PropCreateSimple(_T(""), true));

		m_grid.InsertItem(2, PropCreateReadOnlyItem(_T(""), _T("Caroline")));
		m_grid.SetSubItem(2, 1, PropCreateSimple(_T(""), false));

		return TRUE;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CLibretro * lib = CLibretro::GetSingleton();
		lib->variables_changed = true;
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
	CListViewCtrl assign;
	CTreeViewCtrl event;
	CButton add;
	CButton remove;
	CEdit   editevent;
	input				   * input;
	dinput                 * di;
	bind_list              * bl;
	bool					 notify_d;
	bool                     have_event;
	dinput::di_event         last_event;
	std::vector< HTREEITEM > tree_items;
	guid_container         * guids;
	// returned to list view control
	enum { IDD = IDD_INPUT };

	BEGIN_MSG_MAP(CInputView)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_DESTROY, OnClose)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		NOTIFY_HANDLER(IDC_LIST_ASSIGN, NM_DBLCLK, NotifyHandler)
		NOTIFY_HANDLER(IDC_LIST_ASSIGN, NM_CLICK, NotifyHandlerClick)
	END_MSG_MAP()

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}


	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		Std_File_Writer_u out2;
		if (!out2.open(input->path))
		{
			input->save(out2);
			out2.close();
		}
		EndDialog(wID);
		return 0;
	}

	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		KillTimer(0x1337);
		if (di)
		{
			delete di;
			di = 0;
		}
		bHandled = true;
		return 0;
	}

	LRESULT NotifyHandlerClick(int idCtrl, LPNMHDR pnmh, BOOL&/*bHandled*/) {
		editevent.SetWindowTextW(L"Double click on a entry to set bindings...");
		return 1;
	}

	LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL&/*bHandled*/) {
	//	KillTimer(0x1337);
		while (!process_events(di->read()))Sleep(10);
		std::tostringstream event_text;
		format_event(last_event, event_text);
		int n = ListView_GetSelectionMark(assign);
		if (n != -1)
		{
			dinput::di_event e;
			unsigned         action;
			unsigned retro_id;
			TCHAR description[64] = { 0 };
			bl->get(n, e, action,description,retro_id);
			remove_from_list(assign, n);
			add_to_list(assign, last_event, action, n,description);
			bl->replace(n, last_event, action,description,retro_id);
			input = input::GetSingleton();
			input->bl = bl;
			input->guids = guids;
		}
		ListView_SetSelectionMark(assign, n);
		editevent.SetWindowTextW(L"");
		//SetTimer(0x1337, 10);
		return 1;
	}


	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		input = input::GetSingleton();
		CLibretro* lib = CLibretro::GetSingleton();
		bl = input->bl;
		guids = input->guids;
		di = create_dinput();
		if (di->open(input->lpDI,m_hWnd, input->guids))
		{
			return 0;
		}


		assign = GetDlgItem(IDC_LIST_ASSIGN);
		event = GetDlgItem(IDC_TREE_ACTIONS);
		editevent = GetDlgItem(IDC_EDIT_EVENT);


		{
			LVCOLUMN lvc;
			memset(&lvc, 0, sizeof(lvc));

			lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;
			lvc.cx = 260;
			lvc.pszText = _T("Event");
			lvc.iSubItem = 0;
			assign.InsertColumn(0, &lvc);

			lvc.cx = 150;
			lvc.pszText = _T("Core action");
			lvc.iSubItem = 1;

			assign.InsertColumn(1, &lvc);
			assign.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
			dinput::di_event e;
			unsigned         action;
			unsigned retro_id;

			for (unsigned i = 0, j = bl->get_count(); i < j; ++i)
			{
				TCHAR description[64] = { 0 };
				bl->get(i, e, action, description,retro_id);
				std::tostringstream event_text;
				format_event(e, event_text);
				add_to_list(assign, e, action,INT_MAX,description);
			}

		}
		editevent.SetFocus();
		SetTimer(0x1337, 10);
		notify_d = false;
		return 0;
	}

	LRESULT OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if (wParam == 0x1337)
		{

			if (process_events(di->read()))
			{
				std::tostringstream event_text;
				format_event(last_event, event_text);
				editevent.SetWindowTextW((LPCTSTR)event_text.str().c_str());
			}
		}

		return 0;
	}

private:


	void format_event(const dinput::di_event & e, std::tostringstream & out)
	{
		
		if (e.type == dinput::di_event::ev_key)
		{
			unsigned scancode = e.key.which;
			if (scancode == 0x45 || scancode == 0xC5) scancode ^= 0x80;
			scancode = ((scancode & 0x7F) << 16) | ((scancode & 0x80) << 17);
			TCHAR text[32];
			if (GetKeyNameText(scancode, text, 32))
				out << text;
		}
		else if (e.type == dinput::di_event::ev_none)
		{
			out << _T("None");
		}
		else if (e.type == dinput::di_event::ev_joy)
		{
			out << di->get_joystick_name(e.joy.serial);
			out << _T(' ');

			if (e.joy.type == dinput::di_event::joy_axis)
			{
				out << _T("axis ") << e.joy.which << _T(' ');
				if (e.joy.axis == dinput::di_event::axis_negative) out << _T('-');
				else out << _T('+');
			}
			else if (e.joy.type == dinput::di_event::joy_button)
			{
				out << _T("button ") << e.joy.which;
			}
			else if (e.joy.type == dinput::di_event::joy_pov)
			{
				out << _T("pov ") << e.joy.which << _T(" ") << e.joy.pov_angle << _T('°');
			}
		}
		else if (e.type == dinput::di_event::ev_xinput)
		{
			out << _T("Xbox360 controller #");
			out << e.xinput.index + 1;
			out << _T(' ');

			if (e.xinput.type == dinput::di_event::xinput_axis)
			{
				if (e.xinput.which & 2) out << _T("right");
				else out << _T("left");
				out << _T(" thumb stick ");
				if (e.xinput.which & 1)
				{
					if (e.xinput.axis == dinput::di_event::axis_positive) out << _T("up");
					else out << _T("down");
				}
				else
				{
					if (e.xinput.axis == dinput::di_event::axis_positive) out << _T("right");
					else out << _T("left");
				}
			}
			else if (e.xinput.type == dinput::di_event::xinput_trigger)
			{
				if (e.xinput.which == 0) out << _T("left");
				else out << _T("right");
				out << _T(" analog trigger");
			}
			else if (e.xinput.type == dinput::di_event::xinput_button)
			{
				static const TCHAR * xinput_button[] = { _T("D-pad up"), _T("D-pad down"), _T("D-pad left"), _T("D-pad right"), _T("Start button"), _T("Back button"), _T("left thumb stick button"), _T("right thumb stick button"), _T("left shoulder button"), _T("right shoulder button"), _T("A button"), _T("B button"), _T("X button"), _T("Y button") };
				out << xinput_button[e.xinput.which];
			}
		}
	}

	bool process_events(std::vector< dinput::di_event > events)
	{
		std::vector< dinput::di_event >::iterator it;
		bool     again;
		unsigned last = 0;
		do
		{
			again = false;
			for (it = events.begin() + last; it < events.end(); ++it)
			{
				if (it->type == dinput::di_event::ev_key)
				{
					if (it->key.type == dinput::di_event::key_up)
					{
						again = true;
						last = it - events.begin();
						events.erase(it);
						break;
					}
				}
				else if (it->type == dinput::di_event::ev_joy)
				{
					if (it->joy.type == dinput::di_event::joy_axis)
					{
						if (it->joy.axis == dinput::di_event::axis_center)
						{
							again = true;
							last = it - events.begin();
							events.erase(it);
							break;
						}
					}
					else if (it->joy.type == dinput::di_event::joy_button)
					{
						if (it->joy.button == dinput::di_event::button_up)
						{
							again = true;
							last = it - events.begin();
							events.erase(it);
							break;
						}
					}
					else if (it->joy.type == dinput::di_event::joy_pov)
					{
						if (it->joy.pov_angle == ~0)
						{
							again = true;
							last = it - events.begin();
							events.erase(it);
							break;
						}
					}
				}
				else if (it->type == dinput::di_event::ev_xinput)
				{
					if (it->xinput.type == dinput::di_event::xinput_axis)
					{
						if (it->xinput.axis == dinput::di_event::axis_center)
						{
							again = true;
							last = it - events.begin();
							events.erase(it);
							break;
						}
					}
					else if (it->xinput.type == dinput::di_event::xinput_trigger ||
						it->xinput.type == dinput::di_event::xinput_button)
					{
						if (it->xinput.button == dinput::di_event::button_up)
						{
							again = true;
							last = it - events.begin();
							events.erase(it);
							break;
						}
					}
				}
			}
		} while (again);

		it = events.end();
		if (it > events.begin())
		{
			--it;
			have_event = true;
			last_event = *it;
			return true;
		}

		return false;
	}

	void add_to_list(HWND w, const dinput::di_event & e, unsigned action, int item2,TCHAR* description)
	{
		std::tstring str;
		{
			std::tostringstream text;
			format_event(e, text);
			str = text.str();
		}
		LVITEM lvi;
		memset(&lvi, 0, sizeof(lvi));
		lvi.mask = LVIF_TEXT;
		lvi.iItem = item2;
		lvi.pszText = (TCHAR *)str.c_str();
		int item = ListView_InsertItem(w, &lvi);
		if (item != -1)
		{
			lvi.iSubItem = 1;
			lvi.pszText = (TCHAR *)description;
			SendMessage(w, LVM_SETITEMTEXT, (WPARAM)item, (LPARAM)(LVITEM *)& lvi);
		}
	}

	void remove_from_list(HWND w, unsigned n)
	{
		ListView_DeleteItem(w, n);
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
		COMMAND_ID_HANDLER_EX(IDC_ABOUT, OnAbout)
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
			input_device = input::CreateInstance(GetModuleHandle(NULL), m_hWnd);
			emulator = CLibretro::CreateInstance(m_hWnd ) ;
			m_haccelerator = AtlLoadAccelerators(IDR_ACCELERATOR1);
			pLoop->AddMessageFilter(this);
			AllocConsole();
			AttachConsole(GetCurrentProcessId());
			freopen("CON", "w", stdout);

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
