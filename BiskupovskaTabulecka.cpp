//--------------------------------------------------------------------
// FILENAME:            BiskupovskaTabulecka.cpp
//
// Copyright(c) 2008 Motorola, Inc. All rights reserved.
// Copyright(c) 2023, 2024 Richard Gr·Ëik - Morc
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
#include "resource.h"


#define MAX_LOADSTRING 100
#define THREAD_TIMEOUT 7000

// Global Variables:
HINSTANCE			g_hInst;					// The current instance
HWND				g_hwndCB;					// The command bar handle
HWND                g_hwnd;						// The main window handle
HMENU				g_hSubMenu;					// the 'File' menu handle
HWND				g_hwndList;					// The list view of GUI

static SHACTIVATEINFO s_sai;

HFONT   g_hfont = NULL;							// The handle for display font
LOGFONT g_lf;									// Font structure

TCHAR   g_pszInfo[50][50];// The list hold the modem info

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass	(HINSTANCE, LPTSTR);
BOOL				InitInstance	(HINSTANCE, int);
LRESULT CALLBACK	WndProc			(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About			(HWND, UINT, WPARAM, LPARAM);
HWND				CreateRpCommandBar(HWND);
void                CreateDisplayFont(LONG lHeight, LONG lWeight, LPTSTR szFont);
void				CreateListView(HWND hDlg);
void				ErrorExit(HWND, UINT, LPTSTR);

HANDLE			hScanner				= NULL;
LPSCAN_BUFFER	lpScanBuffer			= NULL;
TCHAR			szScannerName[MAX_PATH] = TEXT("SCN1:");	// default scanner name
DWORD			dwScanSize				= 7095;				// default scan buffer size	
DWORD			dwScanTimeout			= 2000;				// default timeout value (0 means no timeout)	
BOOL			bRequestPending			= FALSE;
BOOL			bStopScanning			= FALSE;

//use sound instead of a beep
BOOL			bUseSound				= TRUE;

#define		countof(x)		sizeof(x)/sizeof(x[0])
enum tagUSERMSGS
{
	UM_INIT	= WM_USER + 0x100,
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

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_WANSAMPLE);

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

    wc.style			= CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;
    wc.lpfnWndProc		= (WNDPROC) WndProc;
    wc.cbClsExtra		= 0;
    wc.cbWndExtra		= 0;
    wc.hInstance		= hInstance;
    wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WANSAMPLE));
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

	LoadString(hInstance, IDC_WANSAMPLE, szWindowClass, MAX_LOADSTRING);
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	hWnd = FindWindow(szWindowClass, szTitle);	
	if (hWnd) 
	{
		SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
		return FALSE;
	} 

	MyRegisterClass(hInstance, szWindowClass);
	
	hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE | WS_NONAVDONEBUTTON ,
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
	TCHAR			szLabelType[10];
	TCHAR			szLen[MAX_PATH];
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
				case IDM_HELP_ABOUT:
					DialogBox(g_hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				    break;

				case IDM_EXIT:
					SendMessage(hWnd, WM_DESTROY, 0, 0);
					break;

				case IDOK:
					SendMessage (hWnd, WM_CLOSE, 0, 0);
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
			hDlg = CreateDialogParam(g_hInst,MAKEINTRESOURCE(IDD_INFOPAGE), hWnd, (DLGPROC)WndProc, NULL);
			g_hSubMenu = (HMENU)SendMessage(g_hwndCB, SHCMBM_GETSUBMENU, 0, IDM_FILE);
			CreateListView(hDlg);
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
					
					wsprintf(szLabelType, TEXT("0x%.2X"), SCNBUF_GETLBLTYP(lpScanBuf));

					wsprintf(szLen, TEXT("%d"), SCNBUF_GETLEN(lpScanBuf));
					break;
			}

			if (GetFocus())
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

			//set CE sound params
			SCAN_PARAMS scan_params;
			memset(&scan_params,0,sizeof(scan_params));
			SI_INIT(&scan_params);
			dwResult = SCAN_GetScanParameters(hScanner,&scan_params);
			SI_SET_FIELD(&scan_params,dwDecodeLedTime,250);
			if(bUseSound)
			{
				SI_SET_STRING(&scan_params,szWaveFile,_T("\\Windows\\Default.wav"));
				SI_SET_FIELD(&scan_params,dwDecodeBeepTime,0);
				SI_SET_FIELD(&scan_params,dwDecodeBeepFrequency,0);
			}
			else
			{
				SI_SET_FIELD(&scan_params,dwDecodeBeepTime,50);
				SI_SET_FIELD(&scan_params,dwDecodeBeepFrequency,2000);
			}
			SCAN_SetScanParameters(hScanner,&scan_params);

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
			CreateDisplayFont(15, 800, _T("MS Sans Serif"));
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
// Function Name: CreateListView
//
// Arguments: the window handle for the dlg which contain the listview
//
// Return Values:
// 
// Description:  create the list view which hold the list of WAN info
// **************************************************************************
LPTSTR testList[] = {_T("this is a text"),_T("this"), NULL};

void CreateListView(HWND hDlg)
{
	LVCOLUMN	lvColumn;
	int			nWidth;
	RECT		rect;
	LPTSTR      *OptionList;
	LV_ITEM     lvI;
	int         i = 0;

	g_hwndList = GetDlgItem(hDlg, IDC_INFO);
	CreateDisplayFont(13, 700, _T("MS Sans Serif"));
	SendMessage(g_hwndList,WM_SETFONT, (WPARAM)g_hfont, (LPARAM) TRUE);

	//generate two titles
	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
    lvColumn.fmt = LVCFMT_LEFT;
    GetWindowRect(g_hwndList,&rect);
   	nWidth = (rect.right - rect.left)/2;
	lvColumn.cx = nWidth - 8;

   	lvColumn.iSubItem = 0;
	lvColumn.pszText = _T("Option");
	ListView_InsertColumn(g_hwndList, 0, &lvColumn);

	lvColumn.iSubItem = 1;
	lvColumn.pszText = _T("Value");
	lvColumn.cx = nWidth + 5;
	ListView_InsertColumn(g_hwndList, 1, &lvColumn);

	OptionList = testList;

	lvI.mask = LVIF_TEXT;
    lvI.iSubItem = 0;

	while(OptionList[i] != NULL)
	{
		lvI.iItem = i;
		lvI.pszText = OptionList[i];
	    lvI.cchTextMax = wcslen(lvI.pszText);
    	ListView_InsertItem(g_hwndList,&lvI);
		i++;
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
//  RETURN VALUE:
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
	MessageBox(NULL, szMsg, NULL, MB_OK);
	SendMessage(hwnd,UM_STOPSCANNING,0,0L);
}
