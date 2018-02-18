#pragma once

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

std::wstring s2ws(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

std::string ws2s(const std::wstring &wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}
#pragma optimize( "", off )  
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
	CLibretro *retro;

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
		return 0;
	}

	LRESULT OnItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPNMPROPERTYITEM pnpi = (LPNMPROPERTYITEM)pnmh;
		int type = pnpi->prop->GetKind();
		LPCTSTR name = pnpi->prop->GetName();
		if (type == 4)
		{
			TCHAR szValue[100] = { 0 };
			pnpi->prop->GetDisplayValue(szValue, sizeof(szValue) / sizeof(TCHAR));
			CComVariant vValue;
			pnpi->prop->GetValue(&vValue);
			vValue.ChangeType(VT_BSTR);
			ATLTRACE(_T("OnItemChanged - Ctrl: %d, Name: '%s', DispValue: '%s', Value: '%ls'\n"),
				idCtrl, pnpi->prop->GetName(), szValue, vValue.bstrVal); idCtrl;

			for (int i = 0; i < retro->variables.size(); i++)
			{
				wstring str = s2ws(retro->variables[i].name);
				if (lstrcmpW(str.c_str(), (LPCTSTR)name) == 0)
				{
					wstring var = szValue;
					string var2 = ws2s(var);
					strcpy(retro->variables[i].var, var2.c_str());
				}

			}
		}
		if (type == 6)
		{
			CComVariant vValue;
			pnpi->prop->GetValue(&vValue);
			vValue.ChangeType(VT_BOOL);
			for (int i = 0; i < retro->variables.size(); i++)
			{
				wstring str = s2ws(retro->variables[i].name);
				wstring str2 = (LPCTSTR)name;
				if (lstrcmp(str.c_str(), str2.c_str()) == 0)
				{
					const char* check = vValue.boolVal ? "enabled" : "disabled";
					strcpy(retro->variables[i].var, check);
				}

			}
		}
		retro->variables_changed = true;
		return 0;
	}


	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		retro = CLibretro::GetSingleton();
		m_grid.SubclassWindow(GetDlgItem(IDC_LIST_VARIABLES));
		m_grid.InsertColumn(0, _T("Option"), LVCFMT_LEFT, 200, 0);
		m_grid.InsertColumn(1, _T("Setting"), LVCFMT_LEFT, 80, 0);
		m_grid.SetExtendedGridStyle(PGS_EX_SINGLECLICKEDIT);

		for (int i = 0; i < retro->variables.size(); i++)
		{
			string str = retro->variables[i].description;
			wstring st2 = s2ws(str);
			string usedv = retro->variables[i].usevars;
			string var = retro->variables[i].var;
			wstring varname = s2ws(retro->variables[i].name);
			m_grid.InsertItem(i, PropCreateReadOnlyItem(_T(""), st2.c_str()));
			if (strcmp(usedv.c_str(), "enabled|disabled") == 0 || strcmp(usedv.c_str(), "disabled|enabled") == 0)
			{
				bool check = strstr((char*)var.c_str(), "enabled");
				m_grid.SetSubItem(i, 1, PropCreateCheckButton(varname.c_str(), check));
			}
			else
			{

				vector <wstring> colour;
				colour.clear();
				char *pch = (char*)retro->variables[i].usevars;
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
					std::wostringstream temp;
					temp << val;
					colour.push_back(temp.str());
					pch += str2 - pch++;

				}
				std::wostringstream temp;
				temp << pch;
				colour.push_back(temp.str());

				LPWSTR **wszArray = new LPWSTR*[colour.size() + 1];
				int j = 0;
				for (j; j < colour.size(); j++) {
					wszArray[j] = new LPWSTR[colour[j].length()];
					lstrcpy((LPWSTR)wszArray[j], colour[j].c_str());

				}
				wszArray[j] = NULL;


				m_grid.SetSubItem(i, 1, PropCreateList(varname.c_str(), (LPCTSTR*)wszArray));
				HPROPERTY hDisabled = m_grid.GetProperty(i, 1);
				TCHAR szValue[100] = { 0 };
				hDisabled->GetDisplayValue(szValue, sizeof(szValue) / sizeof(TCHAR));
				wstring variant = s2ws(retro->variables[i].var);
				CComVariant vValue(variant.c_str());
				vValue.ChangeType(VT_BSTR);
				m_grid.SetItemValue(hDisabled, &vValue);

				for (j = 0; j < colour.size(); j++) {
					free(wszArray[j]);
				}
				free(wszArray);
			}
		}

		return TRUE;
	}

	void save()
	{
		FILE *fp = NULL;
		ini_t* ini = ini_create(NULL);
		for (int i = 0; i < retro->variables.size(); i++) {
			ini_property_add(ini, INI_GLOBAL_SECTION, retro->variables[i].name, strlen(retro->variables[i].name), retro->variables[i].var,
				strlen(retro->variables[i].var));
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

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		save();
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