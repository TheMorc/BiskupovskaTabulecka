//--------------------------------------------------------------------
// FILENAME:            BiskupovskaTabulecka.cpp
//
// Copyright(c) 2008 Motorola, Inc. All rights reserved.
// Copyright(c) 2023, 2024 Richard Gráèik - Morc
//
// DESCRIPTION:     Defines the entry point for the application.
//
// PLATFORMS:       Windows CE
//
// NOTES:           Public
//
// 
//--------------------------------------------------------------------


#include <windows.h>
#include <commctrl.h>
#include <ScanCAPI.h>
#include <aygshell.h>
#include <wininet.h>
#include <msxml2.h>
#include <comutil.h>
#include <vector>
#include <set>

#include "resource.h"

#define MAX_LOADSTRING 100
#define THREAD_TIMEOUT 7000

// Global Variables:
HINSTANCE			g_hInst;					// The current instance
HWND				g_hwndCB;					// The command bar handle
HWND                g_hwnd;						// The main window handle
HMENU				g_hSubMenu;					// the 'File' menu handle
HWND				g_hwndList;					// The list view of GUI
HWND				g_hwndEntryBox;				// The entrybox handle for add/edit dlg

static SHACTIVATEINFO s_sai;

HFONT   g_hfont = NULL;							// The handle for display font
LOGFONT g_lf;									// Font structure

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass	(HINSTANCE, LPTSTR);
BOOL				InitInstance	(HINSTANCE, int);
LRESULT CALLBACK	WndProc			(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About			(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	Settings		(HWND, UINT, WPARAM, LPARAM);

//add/edit entrybox related
LRESULT CALLBACK	AddEntry		(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	EditEntry		(HWND, UINT, WPARAM, LPARAM);
LRESULT				AddEditEntryBoxNotifyHandler(HWND, UINT, WPARAM, LPARAM);
TCHAR entryBoxTemp[2000];
TCHAR entryBoxURLGen[65535];
TCHAR uploadURLFinal[65535];
int selectedDevice = 255;
void	PatchEntry(HWND hDlg, bool addingMode);
void	MouseHack(BOOL wait);
bool entryBoxRetry = false;

HWND				CreateRpCommandBar(HWND);
void                CreateDisplayFont(LONG lHeight, LONG lWeight, LPTSTR szFont);
void				CreateListView(HWND hDlg);
void				ErrorExit(HWND, UINT, LPTSTR);
void				SetScanParams();
TCHAR* HTTPReq(wchar_t*);

HANDLE			hScanner				= NULL;
LPSCAN_BUFFER	lpScanBuffer			= NULL;
TCHAR			szScannerName[MAX_PATH] = TEXT("SCN1:");	// default scanner name
DWORD			dwScanSize				= 7095;				// default scan buffer size	
DWORD			dwScanTimeout			= 2000;				// default timeout value (0 means no timeout)	
BOOL			bRequestPending			= FALSE;
BOOL			bStopScanning			= FALSE;

//table specific
void		RequestTable(HWND hDlg);
std::set<_bstr_t> deviceColumns;
std::vector<_bstr_t> deviceData;
int deviceDataCount, deviceCount		= 0;
int columnCount							= 0;
BOOL tableRefresh						= FALSE;
BOOL firstLoad							= TRUE;
BOOL tableRefreshFailed					= FALSE;

//REGISTRY DEFAULT VALUES
BOOL bUseSound							= FALSE;
_bstr_t downloadURL						= _bstr_t("http://192.168.1.3/tabulecka_xml.php");
_bstr_t uploadURL						= _bstr_t("http://192.168.1.3/tabulecka_edit.php");

#define		countof(x)		sizeof(x)/sizeof(x[0])
enum tagUSERMSGS
{
	UM_SCAN	= WM_USER + 0x200,
	UM_STARTSCANNING,
	UM_STOPSCANNING
};

int WINAPI WinMain(	HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					LPTSTR    lpCmdLine,
					int       nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_BISKUPOVSKATABULECKA);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}

// **************************************************************************
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    It is important to call this function so that the application 
//    will get 'well formed' small icons associated with it.
// **************************************************************************
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
	WNDCLASS	wc;

    wc.style			= CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc		= (WNDPROC) WndProc;
    wc.cbClsExtra		= 0;
    wc.cbWndExtra		= 0;
    wc.hInstance		= hInstance;
	wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BISKUPOVSKATABULECKA));
    wc.hCursor			= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName		= 0;
    wc.lpszClassName	= szWindowClass;

	return RegisterClass(&wc);
}

// **************************************************************************
//  FUNCTION: InitInstance(HANDLE, int)
//
//  PURPOSE: Saves instance handle and creates main window
//
//  COMMENTS:
//
//    In this function, we save the instance handle in a global variable and
//    create and display the main program window.
// **************************************************************************
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND	hWnd = NULL;
	TCHAR	szTitle[MAX_LOADSTRING];			// The title bar text
	TCHAR	szWindowClass[MAX_LOADSTRING];		// The window class name

	g_hInst = hInstance;		// Store instance handle in our global variable

	//set beachball to entertain user while the app is loading
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	
	LoadString(hInstance, IDC_BISKUPOVSKATABULECKA, szWindowClass, MAX_LOADSTRING);
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	hWnd = FindWindow(szWindowClass, szTitle);	
	if (hWnd) 
	{
		SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
		return FALSE;
	} 

	MyRegisterClass(hInstance, szWindowClass);
	
	hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE ,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
	if (!hWnd)
	{	
		return FALSE;
	}

	g_hwnd = hWnd;

	if (g_hwndCB)
    {
		RECT rc;
        RECT rcMenuBar;

		GetWindowRect(hWnd, &rc);
        GetWindowRect(g_hwndCB, &rcMenuBar);
		rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
		
		MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
	}

	//registry shitzzzz
	HKEY key;
	DWORD keyDisp;

	BYTE UseSoundReg = bUseSound;
	
	LONG keyStatus = RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\BiskupovskaTabulecka"), 0, NULL,
		REG_OPTION_NON_VOLATILE, 0, NULL, &key, &keyDisp);
	if (keyStatus != ERROR_SUCCESS)
	{
		OutputDebugString(_T("reg create+open failed\n"));
		MessageBox(hWnd, _T("RegCreateKey failed"), _T("Tabuleèka"), MB_OK | MB_ICONERROR);
	}
	if (keyDisp == REG_CREATED_NEW_KEY)
	{
		OutputDebugString(_T("reg writing default values\n"));
		
		LONG regWrite = RegSetValueEx(key, _T("UseSound"), 0, REG_DWORD, &UseSoundReg, 1);
		if (regWrite == ERROR_SUCCESS)
			MessageBox(hWnd, _T("First run, loading default settings"), _T("Tabuleèka"), MB_OK | MB_ICONERROR);
		else
			MessageBox(hWnd, _T("Loading default settings failed"), _T("Tabuleèka"), MB_OK | MB_ICONERROR);

		//god, I hate these conversions
		const BYTE* BYTEDlURLKey = reinterpret_cast<const BYTE*>(downloadURL.GetBSTR());
		int BYTEDlURLKeyLength = SysStringByteLen(downloadURL.GetBSTR());
		const BYTE* BYTEUpURLKey = reinterpret_cast<const BYTE*>(uploadURL.GetBSTR());
		int BYTEUpURLKeyLength = SysStringByteLen(uploadURL.GetBSTR());

		RegSetValueEx(key, _T("DownloadURL"), 0, REG_SZ, BYTEDlURLKey, BYTEDlURLKeyLength);
		RegSetValueEx(key, _T("UploadURL"), 0, REG_SZ, BYTEUpURLKey, BYTEUpURLKeyLength);
	}
	else if (keyDisp == REG_OPENED_EXISTING_KEY)
	{
		OutputDebugString(_T("reg reading values\n"));
		DWORD regData;
		LONG regRead = RegQueryValueEx(key, _T("UseSound"), 0, NULL, &UseSoundReg, &regData);
		bUseSound = (regData == 1) ? TRUE : FALSE;


		//god, why
		TCHAR downloadURLTCHAR[255];
		unsigned long downloadURLdataLength = sizeof(char) * 255;
		RegQueryValueEx(key, _T("DownloadURL"), 0, NULL, (LPBYTE)downloadURLTCHAR,
			&downloadURLdataLength);
		downloadURL = _bstr_t(downloadURLTCHAR);

		TCHAR uploadURLTCHAR[255];
		unsigned long uploadURLdataLength = sizeof(char) * 255;
		RegQueryValueEx(key, _T("UploadURL"), 0, NULL, (LPBYTE)uploadURLTCHAR, &uploadURLdataLength);
		uploadURL = _bstr_t(uploadURLTCHAR);
	}
	RegCloseKey(key);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

// **************************************************************************
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
// **************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent, nRet = 0;
	DWORD			dwResult;
	TCHAR			szMsgBuf[256];
	LPSCAN_BUFFER	lpScanBuf;
	static HWND hDlg;

	switch (message) 
	{
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{	
				case IDM_ABOUT:
					DialogBox(g_hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				    break;

				case IDM_SETTINGS:
					DialogBox(g_hInst, (LPCTSTR)IDD_SETTINGS, hWnd, (DLGPROC)Settings);
				    break;

				case IDM_ADDNEWENTRY:
					DialogBox(g_hInst, (LPCTSTR)IDD_ADDEDIT, hWnd, (DLGPROC)AddEntry);
					break;

				case IDM_EDITSELECTED:
				case IDM_EDIT:
					DialogBox(g_hInst, (LPCTSTR)IDD_ADDEDIT, hWnd, (DLGPROC)EditEntry);
					break;

				case IDCLOSE:
				case IDCANCEL:
				case IDOK:
				case IDM_EXIT:
					SendMessage(hWnd, WM_DESTROY, 0, 0);
					SendMessage(hWnd, WM_CLOSE, 0, 0);
					break;

				case IDM_REFRESH:
					SetCursor(LoadCursor(NULL, IDC_WAIT));
					deviceColumns.clear();
					deviceData.clear();
					deviceDataCount = 0;
					deviceCount = 0;
					tableRefreshFailed = FALSE;
					RequestTable(hDlg);
					ListView_DeleteAllItems(g_hwndList);
					for(int col = 0; col <= columnCount; col++)
						//this actually still leaves three columns in the lv, workaround in UpdateListView()
						ListView_DeleteColumn(g_hwndList, col);
					columnCount = 0;
					tableRefresh = TRUE;
					CreateListView(hDlg);
					SetCursor(LoadCursor(NULL, IDC_ARROW));
					break;

				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;

		case WM_CREATE:
			g_hwndCB = CreateRpCommandBar(hWnd);
			PostMessage(hWnd,UM_STARTSCANNING,0,0L);
            memset (&s_sai, 0, sizeof (s_sai));
            s_sai.cbSize = sizeof (s_sai);
			hDlg = CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_INFOPAGE), hWnd, (DLGPROC)WndProc,
				NULL);
			g_hSubMenu = (HMENU)SendMessage(g_hwndCB, SHCMBM_GETSUBMENU, 0, IDM_MENU);
			break;

		case WM_PAINT:
			break;

		case WM_DESTROY:
			EndDialog(hDlg, nRet);
			if (g_hfont != NULL)
    			DeleteObject(g_hfont);
			CommandBar_Destroy(g_hwndCB);

			PostQuitMessage(0);
			break;

		case WM_ACTIVATE:
            // Notify shell of our activate message
			SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);

			if (firstLoad)
			{
				RequestTable(hDlg);
				CreateListView(hDlg);
				SetCursor(LoadCursor(NULL, IDC_ARROW));
				firstLoad = FALSE;
			}

            if(LOWORD(wParam) != WA_INACTIVE)
            {
				SetFocus(g_hwndList);
            }

			switch(LOWORD(wParam))
			{
				case WA_INACTIVE:
					if (bRequestPending)
						dwResult = SCAN_Flush(hScanner);
					break;
				
				default:

					if (!bRequestPending && lpScanBuffer != NULL && !bStopScanning)
					{	
						dwResult = SCAN_ReadLabelMsg(hScanner,
													 lpScanBuffer,
													 hWnd,
													 UM_SCAN,
													 dwScanTimeout,
													 NULL);
						
						if ( dwResult != E_SCN_SUCCESS )
							ErrorExit(hWnd, IDS_SCAN_FAIL, TEXT("SCAN_ReadLabelMsg"));
						else
							bRequestPending = TRUE;
					}
					break;
			}

     		break;

		case WM_SETTINGCHANGE:
			SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
     		break;
		
		case WM_CLOSE:
			break;

		case WM_SIZE:
			{
			INT nWidth = LOWORD(lParam);
			INT nHeight = HIWORD(lParam);
			MoveWindow(g_hwndList, -3, -5, nWidth+8, nHeight+9, TRUE);
			}
			break;

		case UM_SCAN:
			bRequestPending = FALSE;

			lpScanBuf = (LPSCAN_BUFFER)lParam;
			if ( lpScanBuf == NULL )
				ErrorExit(hWnd, IDS_SCAN_FAIL, 0);

			switch (SCNBUF_GETSTAT(lpScanBuf))
			{ 
				case E_SCN_READCANCELLED:

					if (bStopScanning)
					{	// complete the second step of UM_STOPSCANNING
						SendMessage(hWnd,UM_STOPSCANNING,0,0L);	
						return TRUE;
					}
					if (!GetFocus())	
						break;	// Do nothing if read was cancelled while deactivation
					break;


				case E_SCN_SUCCESS:
					wsprintf(szMsgBuf, (LPTSTR)SCNBUF_GETDATA(lpScanBuffer));
					//OutputDebugString(szMsgBuf);
					int scan;
					try
					{
						scan = _ttoi(szMsgBuf);
					}
					catch (int)
					{
						MessageBox(g_hwnd, _T("Scan ID parser failed"), _T("Tabuleèka"), MB_OK | MB_ICONERROR);
					}
					int actualDeviceID = (deviceCount-scan);
					ListView_EnsureVisible(g_hwndList, actualDeviceID, FALSE);
					ListView_SetItemState(g_hwndList, ListView_GetSelectionMark(g_hwndList), 0,
						LVIS_SELECTED);
					ListView_SetSelectionMark(g_hwndList, actualDeviceID);
					ListView_SetItemState(g_hwndList, actualDeviceID, LVIS_SELECTED, LVIS_SELECTED);
					break;
			}

			if (GetFocus())
			{
				dwResult = SCAN_ReadLabelMsg(hScanner,
											 lpScanBuffer,
											 hWnd,
											 message,
											 dwScanTimeout,
											 NULL);
				
				if ( dwResult != E_SCN_SUCCESS )
					ErrorExit(hWnd, IDS_SCAN_FAIL, TEXT("SCAN_ReadLabelMsg"));
				else
					bRequestPending = TRUE;
			}

			return TRUE;

		case UM_STARTSCANNING:

			dwResult = SCAN_Open(szScannerName, &hScanner);
			if ( dwResult != E_SCN_SUCCESS )
			{	
				ErrorExit(hWnd, IDS_COMMAND1, TEXT("SCAN_Open"));
				break;
			}

			dwResult = SCAN_Enable(hScanner);
			if ( dwResult != E_SCN_SUCCESS )
			{
				ErrorExit(hWnd, IDS_COMMAND1, TEXT("SCAN_Enable"));
				break;
			}

			SetScanParams();

			lpScanBuffer = SCAN_AllocateBuffer(TRUE, dwScanSize);
			if (lpScanBuffer == NULL)
			{
				ErrorExit(hWnd, IDS_COMMAND1, TEXT("SCAN_AllocateBuffer"));
				return TRUE;
			}

			dwResult = SCAN_ReadLabelMsg(hScanner,
										 lpScanBuffer,
										 hWnd,
										 UM_SCAN,
										 dwScanTimeout,
										 NULL);
			if ( dwResult != E_SCN_SUCCESS )
				ErrorExit(hWnd, IDS_COMMAND1, TEXT("SCAN_ReadLabelMsg"));
			else
				bRequestPending = TRUE;
			break;

			return TRUE;

		case UM_STOPSCANNING:
			if (!bStopScanning && bRequestPending)													
				SCAN_Flush(hScanner);

			if (!bRequestPending)			
			{							 
				SCAN_Disable(hScanner);

				if (lpScanBuffer)
					SCAN_DeallocateBuffer(lpScanBuffer);

				SCAN_Close(hScanner);

				EndDialog(hWnd, 0);
			}
			bStopScanning = TRUE;

			return TRUE;

		default:
				return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// **************************************************************************
// Function Name: CreateRpCommandBar
//
// Arguments: The window handle to hold the menu bar
//
// Return Values: The Window Handle of the Menu Bar
// 
// Description:  To create the menu
// **************************************************************************
HWND CreateRpCommandBar(HWND hwnd)
{
	SHMENUBARINFO mbi;

	memset(&mbi, 0, sizeof(SHMENUBARINFO));
	mbi.cbSize     = sizeof(SHMENUBARINFO);
	mbi.hwndParent = hwnd;
	mbi.nToolBarId = IDM_MENU;
	mbi.hInstRes   = g_hInst;
	mbi.dwFlags	   = SHCMBF_HMENU;
	mbi.nBmpId     = 0;
	mbi.cBmpImages = 0;

	if (!SHCreateMenuBar(&mbi)) 
		return NULL;

	return mbi.hwndMB;
}

// **************************************************************************
// Function Name: About
//
// Arguments: Please refer to the MSDN help
//
// Return Values:
// 
// Description:  call back function for About Box
// **************************************************************************
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	SHINITDLGINFO shidi;
	TCHAR		szVersion[MAX_LOADSTRING];

	switch (message)
	{
		case WM_INITDIALOG:
			// Create a Done button and size it.  
			shidi.dwMask = SHIDIM_FLAGS;
			shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN;
			shidi.hDlg = hDlg;
			SHInitDialog(&shidi);
			
			LoadStringW(g_hInst, IDS_VERSION, szVersion, MAX_LOADSTRING);
			SetDlgItemTextW(hDlg, IDC_VERSION, szVersion);
			CreateDisplayFont(30, 800, _T("MS Sans Serif"));
        	SendMessageW(GetDlgItem(hDlg, IDC_APPNAME),WM_SETFONT, (WPARAM)g_hfont, (LPARAM) TRUE);
			return TRUE; 

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			} 
			break;
	}
    return FALSE;
}

// **************************************************************************
// Function Name: Settings
//
// Arguments: Please refer to the MSDN help
//
// Return Values:
// 
// Description:  call back function for settings dialog popup
// **************************************************************************
LRESULT CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
			{
			//set checkmark to the current Use Sound state 
			CheckDlgButton(hDlg, IDC_USESOUND, bUseSound);

			//set smaller font to edit controls so we can fit more text
			CreateDisplayFont(18, 500, _T("MS Sans Serif"));
        	SendMessageW(GetDlgItem(hDlg, IDC_DOWNEDIT),WM_SETFONT, (WPARAM)g_hfont, (LPARAM) TRUE);
        	SendMessageW(GetDlgItem(hDlg, IDC_UPEDIT),WM_SETFONT, (WPARAM)g_hfont, (LPARAM) TRUE);

			//not these conversions again, whyyy
			HWND downloadEdit = GetDlgItem(hDlg, IDC_DOWNEDIT);
			BSTR downloadEditBSTR = SysAllocString(downloadURL);
			LPCWSTR downloadEditText = static_cast<LPCWSTR>(downloadEditBSTR);
			SetWindowText(downloadEdit, downloadEditText);

			HWND uploadEdit = GetDlgItem(hDlg, IDC_UPEDIT);
			BSTR uploadEditBSTR = SysAllocString(uploadURL);
			LPCWSTR uploadEditText = static_cast<LPCWSTR>(uploadEditBSTR);
			SetWindowText(uploadEdit, uploadEditText);
			return TRUE; 
			}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{

				case IDOK:
					HKEY key;
					DWORD keyDisp;
					BYTE UseSoundReg = 0;
					HWND useSoundHandle = GetDlgItem(hDlg, IDC_USESOUND);
					if(SendMessage(useSoundHandle, BM_GETCHECK, 0,0))
					{
						OutputDebugString(_T("usesound checked\n"));
						UseSoundReg = 1;
						bUseSound = TRUE;
					}
					else
					{
						bUseSound = FALSE;
					}


					//what did I do to you lol
					//what even is this lol, LPWSTR > TCHAR > _bstr_t > BYTE
					HWND downloadEdit = GetDlgItem(hDlg, IDC_DOWNEDIT);
					TCHAR downloadEditTCHAR[255];
					unsigned long downloadEditdataLength = sizeof(char) * 255;
					GetWindowText(downloadEdit, downloadEditTCHAR, downloadEditdataLength);
					_bstr_t downloadEditBSTR = _bstr_t(downloadEditTCHAR);
					const BYTE* downloadEditBYTE = reinterpret_cast<const BYTE*>(downloadEditBSTR.GetBSTR());
					int downloadEditBYTELength = SysStringByteLen(downloadEditBSTR.GetBSTR());

					HWND uploadEdit = GetDlgItem(hDlg, IDC_UPEDIT);
					TCHAR uploadEditTCHAR[255];
					unsigned long uploadEditdataLength = sizeof(char) * 255;
					GetWindowText(uploadEdit, uploadEditTCHAR, uploadEditdataLength);
					_bstr_t uploadEditBSTR = _bstr_t(uploadEditTCHAR);
					const BYTE* uploadEditBYTE = reinterpret_cast<const BYTE*>(uploadEditBSTR.GetBSTR());
					int uploadEditBYTELength = SysStringByteLen(uploadEditBSTR.GetBSTR());

					downloadURL = downloadEditBSTR;
					uploadURL = uploadEditBSTR;


					SetScanParams();
					
					// write entries
					RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\BiskupovskaTabulecka"), 0, NULL, REG_OPTION_NON_VOLATILE, 0, NULL, &key, &keyDisp);
					RegSetValueEx(key, _T("UseSound"), 0, REG_DWORD, &UseSoundReg, 1);
					RegSetValueEx(key, _T("DownloadURL"), 0, REG_SZ, downloadEditBYTE, downloadEditBYTELength);
					RegSetValueEx(key, _T("UploadURL"), 0, REG_SZ, uploadEditBYTE, uploadEditBYTELength);
					RegCloseKey(key);

				EndDialog(hDlg, 1);
				return TRUE;
			
			} 
			break;
	}
    return FALSE;
}

// **************************************************************************
// Function Name: AddEntry
//
// Arguments: Please refer to the MSDN help
//
// Return Values:
// 
// Description:  call back function for addentry dialog popup
// **************************************************************************
LRESULT CALLBACK AddEntry(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND g_hwndEntryBoxLocal;

	switch (message)
	{
		case WM_INITDIALOG:
		{
			EnableWindow(g_hwndList, false);
			EnableWindow(g_hwnd, false);

			int selectedDeviceLocal = ListView_GetSelectionMark(g_hwndList);
			SetWindowText(hDlg, _T("Adding new entry N/A"));
		
			g_hwndEntryBoxLocal = CreateWindow(WC_LISTVIEW, NULL, WS_VISIBLE | LVS_REPORT | LVS_EDITLABELS, 0, 0, 480, 420, hDlg, (HMENU) 500, g_hInst, NULL);
			ListView_SetExtendedListViewStyle(g_hwndEntryBoxLocal, ListView_GetExtendedListViewStyle(g_hwndEntryBoxLocal) | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
			LVCOLUMN	lvColumn;
			lvColumn.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_WIDTH | LVCF_TEXT;
			lvColumn.fmt = LVCFMT_LEFT;
			lvColumn.pszText = _T("Device entry");
			lvColumn.iSubItem = 0;
			lvColumn.cx = 2000;
			ListView_InsertColumn(g_hwndEntryBoxLocal, 0, &lvColumn);

			g_hwndEntryBox = g_hwndEntryBoxLocal; //HACK! HACK!
			selectedDevice = selectedDeviceLocal; //HACH!!!!! WHAT?????!??!

			//this part of the code is really interesting due to quirks on the MSVC/WIN32/WM/CE kernel side
			//it's chaotic evil, HACK!!! HACK!!!
			LVITEM lvI;
			lvI.mask = LVIF_TEXT | LVIF_STATE;
			lvI.iItem = 0;
			lvI.cchTextMax = 31999;
			lvI.state = 0;
			lvI.stateMask = 0;
			for	(int i = 0; i < columnCount; i++)
			{
				//printf("columnIndex %d, data: %s\n", i, deviceData[selectedDevice *columnCount + i].GetBSTR());
				ListView_InsertItem(g_hwndEntryBoxLocal, &lvI);
			}
			std::set<_bstr_t>::iterator it;
			int counter = 0;
			for (it = deviceColumns.begin(); it != deviceColumns.end(); ++it, ++counter){
				_bstr_t column = *it;
				OutputDebugString(column.GetBSTR()); //HACK!!!!!
				_bstr_t colText = column.GetBSTR()+2; //HACK!!!!
				ListView_SetItemText(g_hwndEntryBoxLocal,counter, 0, colText);
			}

			return TRUE; 
		}

		case WM_NOTIFY: //listview change handling, uses the HACK above!
		{
			LV_DISPINFO *pLvdi = (LV_DISPINFO *)lParam;
			switch(pLvdi->hdr.code)
			{
				case LVN_ENDLABELEDIT:
					if ((pLvdi->item.iItem != -1) && (pLvdi->item.pszText != NULL))
						ListView_SetItemText(g_hwndEntryBox,ListView_GetSelectionMark(g_hwndEntryBox), 0, pLvdi->item.pszText);
					return true;
			}
		}

		case WM_COMMAND:
			switch (wParam)
			{
				case IDOK:	
					printf("preparing to save changes!");
					MouseHack(true);
					entryBoxRetry = false;
					wsprintf(entryBoxURLGen, _T("?add"), selectedDevice);
					for	(int i = 0; i < columnCount; i++)
					{
						//printf("columnIndex %d, data: %s\n", i, entryBoxTemp);
						ListView_GetItemText(g_hwndEntryBox, i, 0, entryBoxTemp, sizeof(entryBoxTemp));
						wsprintf(entryBoxURLGen, _T("%s&%d=%s"), entryBoxURLGen, i, entryBoxTemp);
					}
					PatchEntry(hDlg, true);
					//OutputDebugString(entryBoxURLGen);
					EnableWindow(g_hwndList, true);
					EnableWindow(g_hwnd, true);
					SendMessage(g_hwnd, WM_COMMAND, IDM_REFRESH, NULL);
					MouseHack(false);
					EndDialog(hDlg, 1);
					break;

				default: //setting title afterwards in a WM_COMMAND as a HACK!!!! Again due to some win32 quirk
					TCHAR entryTitle[25];
					wsprintf(entryTitle, _T("Adding new entry %d"), deviceCount+1);
					SetWindowText(hDlg, entryTitle);
					break;
			} 
			break;
	}
    return FALSE;
}

// **************************************************************************
// Function Name: EditEntry
//
// Arguments: Please refer to the MSDN help
//
// Return Values:
// 
// Description:  call back function for editentry dialog popup
// **************************************************************************
LRESULT CALLBACK EditEntry(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{	
	HWND g_hwndEntryBoxLocal;

	switch (message)
	{
		case WM_INITDIALOG:
		{
			EnableWindow(g_hwndList, false);
			EnableWindow(g_hwnd, false);

			int selectedDeviceLocal = ListView_GetSelectionMark(g_hwndList);
			SetWindowText(hDlg, _T("Editing entry N/A"));
		
			g_hwndEntryBoxLocal = CreateWindow(WC_LISTVIEW, NULL, WS_VISIBLE | LVS_REPORT | LVS_EDITLABELS, 0, 0, 480, 420, hDlg, (HMENU) 500, g_hInst, NULL);
			ListView_SetExtendedListViewStyle(g_hwndEntryBoxLocal, ListView_GetExtendedListViewStyle(g_hwndEntryBoxLocal) | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
			LVCOLUMN	lvColumn;
			lvColumn.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_WIDTH | LVCF_TEXT;
			lvColumn.fmt = LVCFMT_LEFT;
			lvColumn.pszText = _T("Device entry");
			lvColumn.iSubItem = 0;
			lvColumn.cx = 2000;
			ListView_InsertColumn(g_hwndEntryBoxLocal, 0, &lvColumn);

			g_hwndEntryBox = g_hwndEntryBoxLocal; //HACK! HACK!
			selectedDevice = selectedDeviceLocal; //HACH!!!!! WHAT?????!??!

			//this part of the code is really interesting due to quirks on the MSVC/WIN32/WM/CE kernel side
			//it's chaotic evil, HACK!!! HACK!!!
			LVITEM lvI;
			lvI.mask = LVIF_TEXT | LVIF_STATE;
			lvI.iItem = 0;
			lvI.cchTextMax = 31999;
			lvI.state = 0;
			lvI.stateMask = 0;
			for	(int i = 0; i < columnCount; i++)
			{
				//printf("columnIndex %d, data: %s\n", i, deviceData[selectedDevice *columnCount + i].GetBSTR());
				ListView_InsertItem(g_hwndEntryBoxLocal, &lvI);
			}
			for	(int i = 0; i < columnCount; i++) //HACK!!!!! HACK!!!
			{
				ListView_SetItemText(g_hwndEntryBoxLocal,i, 0, deviceData[selectedDeviceLocal *columnCount + i].GetBSTR());
			}

			return TRUE; 
		}

		case WM_NOTIFY: //listview change handling, uses the HACK above!
		{
			LV_DISPINFO *pLvdi = (LV_DISPINFO *)lParam;
			switch(pLvdi->hdr.code)
			{
				case LVN_ENDLABELEDIT:
					if ((pLvdi->item.iItem != -1) && (pLvdi->item.pszText != NULL))
						ListView_SetItemText(g_hwndEntryBox,ListView_GetSelectionMark(g_hwndEntryBox), 0, pLvdi->item.pszText);
					return true;
			}
		}

		case WM_COMMAND:
			switch (wParam)
			{
				case IDOK:	
					printf("preparing to save changes!");
					MouseHack(true);
					entryBoxRetry = false;
					wsprintf(entryBoxURLGen, _T("?id=%d"), selectedDevice);
					for	(int i = 0; i < columnCount; i++)
					{
						//printf("columnIndex %d, data: %s\n", i, entryBoxTemp);
						ListView_GetItemText(g_hwndEntryBox, i, 0, entryBoxTemp, sizeof(entryBoxTemp));
						wsprintf(entryBoxURLGen, _T("%s&%d=%s"), entryBoxURLGen, i, entryBoxTemp);
					}
					PatchEntry(hDlg, false);
					//OutputDebugString(entryBoxURLGen);
					EnableWindow(g_hwndList, true);
					EnableWindow(g_hwnd, true);
					SendMessage(g_hwnd, WM_COMMAND, IDM_REFRESH, NULL);
					MouseHack(false);
					EndDialog(hDlg, 1);
					break;

				default: //setting title afterwards in a WM_COMMAND as a HACK!!!! Again due to some win32 quirk
					TCHAR entryTitle[25];
					wsprintf(entryTitle, _T("Editing entry %d"), selectedDevice);
					SetWindowText(hDlg, entryTitle);
					break;
			} 
			break;
	}
    return FALSE;
}


// **************************************************************************
// Function Name: PatchEntry
//
// Arguments: handle, adding mode
//
// Return Values:
// 
// Description:  patches the entry in tabuleèka
// **************************************************************************
void PatchEntry(HWND hDlg, bool addingMode)
{
	if(!entryBoxRetry)
		wsprintf(uploadURLFinal, _T("%s%s"), uploadURL.GetBSTR(), entryBoxURLGen);
	//OutputDebugString(uploadURLFinal);

	DWORD dwSize = 0;
	char *lpBufferA;
	TCHAR *lpBufferW;

	HINTERNET hInternet = InternetOpen(L"HTTP Request", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if (hInternet)
	{
		HINTERNET hConnect = InternetOpenUrl(hInternet, uploadURLFinal, NULL, 0, INTERNET_FLAG_RELOAD, 0);

		if (hConnect)
		{
			
			lpBufferA = new CHAR [32000];
			
			if (InternetReadFile(hConnect, (LPVOID)lpBufferA, 31999, &dwSize))
			{
				if(dwSize != 0)
				{
					lpBufferA [dwSize] = '\0';
					dwSize = MultiByteToWideChar (CP_ACP, 0, lpBufferA, -1, NULL, 0);
					lpBufferW = new TCHAR [dwSize];
					MultiByteToWideChar (CP_ACP, 0, lpBufferA, -1, lpBufferW, dwSize);
				}
			}

			InternetCloseHandle(hConnect);
		}
		InternetCloseHandle(hInternet);
	}

	lpBufferW[3] = '\0';
	if (_tcscmp(lpBufferW, _T("200")) != 0){
		TCHAR szMsg[256];
		if(addingMode)
			wsprintf(szMsg, TEXT("Adding this entry failed!\nCode: %s"), lpBufferW);
		else
			wsprintf(szMsg, TEXT("Editing this entry failed!\nCode: %s"), lpBufferW);
		if (MessageBox(hDlg, szMsg, _T("Tabuleèka"), MB_RETRYCANCEL | MB_ICONERROR) == IDRETRY){
			entryBoxRetry = true;
			PatchEntry(hDlg, addingMode);
		}
	}
}

// **************************************************************************
// Function Name: MouseHack
//
// Arguments: bool
//
// Return Values:
// 
// Description:  HACK!!!!!! another win32/compiler quirk
// **************************************************************************
void MouseHack(BOOL wait)
{
	//HACK!)!!!!
	if (wait)
		SetCursor(LoadCursor(NULL, IDC_WAIT));
	else
		SetCursor(LoadCursor(NULL, IDC_ARROW));
}

// **************************************************************************
// Function Name: RequestTable
//
// Arguments: dialog handle
//
// Return Values:
// 
// Description:  loads the xml table from url and parses it for later usage
// **************************************************************************
void RequestTable(HWND hDlg)
{
	//basic httprequest no xml parse
	//
	//TCHAR* xmlData = HTTPReq(_T("http://192.168.1.125/tabulecka_xml.php"));
	//std::wstring wStr(xmlData);
	//LPWSTR stringTest = const_cast<LPWSTR>(wStr.c_str());

	//IXMLHTTPRequest hanging/other problems
	//
	//HRESULT hr;
	//IXMLHTTPRequest* xmlReq;
	//hr = CoInitializeEx(NULL,COINIT_MULTITHREADED);
	//hr = CoCreateInstance(CLSID_XMLHTTP30, NULL, CLSCTX_INPROC, IID_IXMLHTTPRequest, (LPVOID *)&xmlReq);
	//hr = xmlReq->open(L"GET", L"http://192.168.1.125/tabulecka_xml.php", _variant_t(FALSE), _variant_t(), _variant_t());
	//_variant_t body;	
	//hr = xmlReq->get_responseBody(body.GetAddress());
	//LPWSTR stringTest = const_cast<LPWSTR>(std::wstring(body).c_str());
	//MessageBox(hDlg, _T("hh"), _T("test"), MB_OK);
	
	//ce works
	IXMLDOMDocument *iXMLDoc		= NULL;
	IXMLDOMElement *iXMLElm			= NULL;
	IXMLDOMNodeList *iXMLChildList	= NULL;
	IXMLDOMNode *iXMLItem			= NULL;
	long lLength = 1;
	short tEmpty;
	_bstr_t bCol;
	_bstr_t bRow;

	_variant_t vXMLSrc;
	HRESULT hr = CoInitializeEx(NULL,COINIT_MULTITHREADED);
	hr = CoCreateInstance (CLSID_DOMDocument, NULL,CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument,
		(LPVOID *)&iXMLDoc);
	if(iXMLDoc){
		if(tableRefreshFailed == FALSE)
		{
			iXMLDoc->put_async(VARIANT_FALSE);
			VariantInit( &vXMLSrc );
			vXMLSrc.vt = VT_BSTR;
			vXMLSrc.bstrVal = SysAllocString(downloadURL);
			
			hr = iXMLDoc->load(vXMLSrc, &tEmpty);
			if (hr == S_OK){
				SysFreeString(vXMLSrc.bstrVal);
				
				iXMLDoc->get_documentElement(&iXMLElm);
				iXMLElm->selectNodes(L"device//",&iXMLChildList);
				iXMLChildList->get_length(&lLength);
				
				for (int x=0;x<lLength;x++){
					iXMLChildList->get_item(x, &iXMLItem);
					iXMLItem->get_nodeName(&bCol.GetBSTR());
					iXMLItem->get_text(&bRow.GetBSTR());
					if (wcscmp(_bstr_t("#text"),bCol) != 0) {
						deviceColumns.insert(bCol.GetBSTR());
						deviceData.push_back(bRow.GetBSTR());
						deviceDataCount++;
						//OutputDebugString(bRow);
					}
				}
			}
			else
			{
				tableRefreshFailed = TRUE;
				MessageBox(g_hwnd, _T("RequestTable download failed\nCheck URL in settings or check your connection"), _T("Tabuleèka"), MB_OK | MB_ICONERROR);
			}
		}
	}
	else
	{
		tableRefreshFailed = TRUE;
		MessageBox(g_hwnd, _T("RequestTable initialization failed"), _T("Tabuleèka"), MB_OK | MB_ICONERROR);
	}
}

// **************************************************************************
// Function Name: CreateListView
//
// Arguments: the window handle for the dlg which contain the listview
//
// Return Values:
// 
// Description:  create the list view which holds the TABULEÈKA!
// **************************************************************************
void CreateListView(HWND hDlg)
{
	LVCOLUMN	lvColumn;
	int			nWidth;
	RECT		rect;

	g_hwndList = GetDlgItem(hDlg, IDC_INFO);

	// Put the list view in full row select mode
	ListView_SetExtendedListViewStyle(g_hwndList, ListView_GetExtendedListViewStyle(g_hwndList) | LVS_EX_FULLROWSELECT);
	
	CreateDisplayFont(21, 700, _T("MS Sans Serif"));
	SendMessage(g_hwndList,WM_SETFONT, (WPARAM)g_hfont, (LPARAM) TRUE);

	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
    lvColumn.fmt = LVCFMT_LEFT;
    GetWindowRect(g_hwndList,&rect);
   	nWidth = (rect.right - rect.left)/2;
	 
	std::set<_bstr_t>::iterator it;
	for (it = deviceColumns.begin(); it != deviceColumns.end(); ++it, ++columnCount){
		_bstr_t column = *it; 
		lvColumn.iSubItem = columnCount;
		lvColumn.pszText = column.GetBSTR()+2;
		int cxModifier = 480/(column.length());		
		int cxModifier2 = ((column.length()-2) <= 5) ? 50 : 0; //-2 on the length to account for array sorting
		lvColumn.cx = nWidth - cxModifier2 - cxModifier;
		if (columnCount < 3 && tableRefresh) //workaround to account for three nonremovable columns when refreshing
			ListView_SetColumn(g_hwndList, columnCount, &lvColumn);
		else	
			ListView_InsertColumn(g_hwndList, columnCount, &lvColumn);
		//OutputDebugString(column.GetBSTR());
	}

	int devDataColumnIndex = 0;
	deviceCount = 0;
	for	(int i = 0; i < deviceDataCount; i++)
	{
		LV_ITEM lvI;
		lvI.mask = LVIF_TEXT | LVIF_STATE;
		lvI.iItem = deviceCount;
		lvI.iSubItem = 0;
		lvI.pszText = _T("0");
		lvI.cchTextMax = 31999;
		lvI.state = 0;
		lvI.stateMask = 0;

		if (devDataColumnIndex == 0){  //HACK!!!!
			ListView_InsertItem(g_hwndList,&lvI);
			ListView_SetItemText(g_hwndList,deviceCount, devDataColumnIndex,  deviceData[i].GetBSTR());
		}else
			ListView_SetItemText(g_hwndList,deviceCount, devDataColumnIndex,  deviceData[i].GetBSTR());

		if (devDataColumnIndex > columnCount-1){
			devDataColumnIndex = 0;
			deviceCount++;
			i--;
		}else{		
			devDataColumnIndex++;
		}
	}
}

// **************************************************************************
// Function Name: CreateDisplayFont
//
// Arguments: lHeight: the height of font (size)
//			  lWeight: the weight of font (Bold or not)
//			  szFont:  the name of the font
//
// Return Values:
// 
// Description: the display font for the GUI
// **************************************************************************
void CreateDisplayFont(LONG lHeight, LONG lWeight, LPTSTR szFont)
{
	//create the font
	memset(&g_lf, 0, sizeof(LOGFONT));          // Clear out structure.
	g_lf.lfHeight =lHeight;                         // Request a 12-pixel-height
	// font.
	lstrcpy(g_lf.lfFaceName, szFont);
	g_lf.lfWeight = lWeight;
	g_hfont = CreateFontIndirect(&g_lf);  // Create the font.
}

LPTSTR LoadMsg(UINT uID, LPTSTR lpBuffer, int nBufSize)
{
	if (!LoadString(g_hInst, uID, lpBuffer, nBufSize))
		wcscpy(lpBuffer, TEXT(""));

	return lpBuffer;
}

//----------------------------------------------------------------------------
//
//  FUNCTION:   ErrorExit(HWND, UINT, LPTSTR)
//
//  PURPOSE:    Handle critical errors and exit dialog. 
//
//  PARAMETERS:
//      hwnd     - handle to the dialog box. 
//      uID		 - ID of the message string to be displayed 
//      szFunc   - function name if it's an API function failure 
//
//  REVALUE:
//      None.
//
//----------------------------------------------------------------------------
void ErrorExit(HWND hwnd, UINT uID, LPTSTR szFunc)
{
	TCHAR szMsg[256];
	TCHAR szBuf[256];

	if (szFunc == NULL)
		wcscpy(szMsg, LoadMsg(uID, szBuf, countof(szBuf)));
	else
		wsprintf(szMsg, TEXT("%s %s"), szFunc, 
					LoadMsg(uID, szBuf, countof(szBuf)));
	MessageBox(NULL, szMsg, _T("Tabuleèka"), MB_OK | MB_ICONERROR);
	SendMessage(hwnd,UM_STOPSCANNING,0,0L);
}

TCHAR* HTTPReq(LPWSTR url)
{
	DWORD dwSize = 0;
	char *lpBufferA;
	TCHAR *lpBufferW;

	HINTERNET hInternet = InternetOpen(L"HTTP Request", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if (hInternet)
	{
		HINTERNET hConnect = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

		if (hConnect)
		{
			
			lpBufferA = new CHAR [32000];
			
			if (InternetReadFile(hConnect, (LPVOID)lpBufferA, 31999, &dwSize))
			{
				if(dwSize != 0)
				{
					lpBufferA [dwSize] = '\0';                 

					dwSize = MultiByteToWideChar (CP_ACP, 0, lpBufferA, -1, NULL, 0);
				 
					lpBufferW = new TCHAR [dwSize];

					MultiByteToWideChar (CP_ACP, 0, lpBufferA, -1, lpBufferW, dwSize);
				}
			}

			InternetCloseHandle(hConnect);
		}
		InternetCloseHandle(hInternet);
	}
	else
	{
	}
	
	return lpBufferW;
}

void SetScanParams()
{
	//set CE sound params
	SCAN_PARAMS scan_params;
	memset(&scan_params,0,sizeof(scan_params));
	SI_INIT(&scan_params);
	DWORD dwResult = SCAN_GetScanParameters(hScanner,&scan_params);
	SI_SET_FIELD(&scan_params,dwDecodeLedTime,250);
	if(bUseSound)
	{
		SI_SET_STRING(&scan_params,szWaveFile,_T("\\Windows\\Default.wav"));
		SI_SET_FIELD(&scan_params,dwDecodeBeepTime,0);
		SI_SET_FIELD(&scan_params,dwDecodeBeepFrequency,0);
	}
	else
	{
		SI_SET_STRING(&scan_params,szWaveFile,_T(""));
		SI_SET_FIELD(&scan_params,dwDecodeBeepTime,25);
		SI_SET_FIELD(&scan_params,dwDecodeBeepFrequency,2000);
	}
	SCAN_SetScanParameters(hScanner,&scan_params);
}