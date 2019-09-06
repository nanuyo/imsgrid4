// IMSGRID.cpp : Defines the entry point for the application.
//

#include <Windows.h>
#include "stdafx.h"
#include "IMSGRID.h"
#include <atlstr.h>
#include <Dbt.h>
#include <ks.h>
#include <ksmedia.h>


// Application
//#define MIPI_JIG
#define CURRENT_MIPI
//#define MIPI_AGING
//#define FOCUS

#ifdef MIPI_JIG
#define CLEAR_MIPI_ERROR_REGISTER   
#define MIPI_CHECK
#define MIPI_CHECK_RETRY 30
#endif

#ifdef CURRENT_MIPI
#define CURRENT_CHECK
#define CURRENT_CHECK_RETRY 50
//#define CLEAR_MIPI_ERROR_REGISTER   
//#define MIPI_CHECK
//#define MIPI_CHECK_RETRY 30
#endif

#ifdef MIPI_AGING
#define MIPI_CHECK
#define CLEAR_MIPI_ERROR_REGISTER 
#define MIPI_CHECK_FOREVER
#endif

#ifdef FOCUS
#define FOCUS_CHECK
#define FOCUS_CHECK_RETRY 100
//#define FOCUS_CHECK_RETRY 1000000
#endif


#define NEW_DLL 
#define MAX_LOADSTRING 100
#define	ID_EDIT_SEND	110
#define	ID_EDIT_RECV	120

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR currentDirectory[MAX_PATH] = { 0 };

GdiplusStartupInput gdiPlusStartupInput;
ULONG_PTR gdiPlusToken;

int monitor_width = 0;
int monitor_height = 0;
int firstDraw = 1;

// FW Version Information
//#define FW_CKECK_INTERVAL 150
#define DF_CODE_VERSION_SIZE 100
int fwCheckCount = 0;
HMODULE hLibrary;
typedef bool(*GetCodeVersionFromDevicePtr)(BYTE* pbyCodeVersion); // Get FW code version from device
GetCodeVersionFromDevicePtr pGetCodeVersionFromDevice;

typedef bool(*ProcReadFromASICPtr)(USHORT Addr, BYTE *pValue);
typedef bool(*ProcWriteToASICPtr)(USHORT Addr, BYTE Value);

ProcReadFromASICPtr m_pProcReadFromASIC;
ProcWriteToASICPtr m_pProcWriteToASIC;


typedef int(*Initialize_DriverInterfacePtr)(int nSenserInitialTime);
typedef bool(*UnInitialize_DriverInterfacePtr)(void);
Initialize_DriverInterfacePtr m_pInitialize_DriverInterface;
UnInitialize_DriverInterfacePtr m_pUnInitialize_DriverInterface;

BYTE pbyCodeVersion[DF_CODE_VERSION_SIZE];
char fwVersion[DF_CODE_VERSION_SIZE] = { 0 };
WCHAR fwVersionW[DF_CODE_VERSION_SIZE] = { 0 };

int fwCheckRes = -1;
//int fwCheckInterval = FW_CKECK_INTERVAL;

#ifdef NEW_DLL
int mipiCheckRes = -1;
int focusCheckRes = -1;
int focusRetry = 0;
int currentCheckRes = -1;
int no_device = -1;

int fwTestDone = -1;
int mipiTestDone = -1;
int currentTestDone = -1;
int focusTestDone = -1;
int allTestDone = -1;

WCHAR dbgMsgBuf[200] = { 0 };
WCHAR mipiValueStr[20] = { 0 };
WCHAR afValueStr[20] = { 0 };
WCHAR afValueStr2[20] = { 0 };

WCHAR afReg0[5] = { 0 };
WCHAR afReg1[5] = { 0 };
WCHAR afReg2[5] = { 0 };
WCHAR afReg3[5] = { 0 };

WCHAR af2Reg0[5] = { 0 };
WCHAR af2Reg1[5] = { 0 };
WCHAR af2Reg2[5] = { 0 };
WCHAR af2Reg3[5] = { 0 };

#endif

// 
DWORD RecvData(LPVOID);
HWND hWndCopy;
HANDLE hFile;
int nSwitch;

int RecvValue = 0;
//LONGLONG RecvTotal = 0;
//LONGLONG RecvCount = 0;
int OffsetValue = 0;
int currentMinValue = 0;
int currentMaxValue = 0;
int drawGrid = 1;
int drawGridBox = 1;

int fwTextX = 100;
int fwTextY = 100;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void				OnPaint(HDC);
void				OnPaint_1(HDC);
void				OnPaint_2(HDC);

HDEVNOTIFY  g_hdevnotify = NULL;

BOOL RegisterForDeviceNotification(HWND hwnd)
{
	DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
	di.dbcc_size = sizeof(di);
	di.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	di.dbcc_classguid = KSCATEGORY_CAPTURE;

	g_hdevnotify = RegisterDeviceNotification(
		hwnd,
		&di,
		DEVICE_NOTIFY_WINDOW_HANDLE
	);

	if (g_hdevnotify == NULL)
	{
		return FALSE;
	}

	return TRUE;
}

#include <Mfobjects.h>
#include <Mfidl.h>
WCHAR      *g_pwszSymbolicLink = NULL;
UINT32     g_cchSymbolicLink = 0;

HRESULT GetSymbolicLink(IMFActivate *pActivate)
{
	return pActivate->GetAllocatedString(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
		&g_pwszSymbolicLink,
		&g_cchSymbolicLink
	);
}
#if 1
HRESULT CheckDeviceLost(DEV_BROADCAST_HDR *pHdr, BOOL *pbDeviceLost)
{
	DEV_BROADCAST_DEVICEINTERFACE *pDi = NULL;

	if (pbDeviceLost == NULL)
	{
		return E_POINTER;
	}

	*pbDeviceLost = FALSE;

	/*if (g_pSource == NULL)
	{
		return S_OK;
	}*/
	if (pHdr == NULL)
	{
		return S_OK;
	}
	if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
	{
		return S_OK;
	}

	// Compare the device name with the symbolic link.

	pDi = (DEV_BROADCAST_DEVICEINTERFACE*)pHdr;

	if (g_pwszSymbolicLink)
	{
		if (_wcsicmp(g_pwszSymbolicLink, pDi->dbcc_name) == 0)
		{
			*pbDeviceLost = TRUE;
		}
	}

	return S_OK;
}
#endif




void IMSLog(LPCTSTR pszStr, ...)
{
	//#ifdef _MY_DEBUG   
	TCHAR szMsg[256];
	va_list args;
	va_start(args, pszStr);
	vswprintf_s(szMsg, 256, pszStr, args);
	OutputDebugString(szMsg);
	//#endif   
}

BOOL FileExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	//Load library
#ifdef NEW_DLL
	hLibrary = ::LoadLibrary(_T("BurnerApLib.dll"));
#else
	hLibrary = ::LoadLibrary(_T("BurnApCustomDLL.dll"));
#endif

	if (hLibrary == NULL) {
		MessageBox(0, L"Load library fail!", L"Error", MB_OK);
		fwCheckRes = 0;
	}

	//Load function from library
#ifdef NEW_DLL
	pGetCodeVersionFromDevice = (GetCodeVersionFromDevicePtr) ::GetProcAddress(hLibrary, "GetCodeVersion");
	m_pProcReadFromASIC = (ProcReadFromASICPtr) ::GetProcAddress(hLibrary, "ProcReadFromASIC");
	m_pProcWriteToASIC = (ProcWriteToASICPtr) ::GetProcAddress(hLibrary, "ProcWriteToASIC");
	m_pInitialize_DriverInterface = (Initialize_DriverInterfacePtr)::GetProcAddress(hLibrary, "Initialize_DriverInterface");
	m_pUnInitialize_DriverInterface = (UnInitialize_DriverInterfacePtr)::GetProcAddress(hLibrary, "UnInitialize_DriverInterface");

#else
	pGetCodeVersionFromDevice = (GetCodeVersionFromDevicePtr) ::GetProcAddress(hLibrary, "GetCodeVersionFromDevice");
#endif

	
	

#ifdef NEW_DLL
	if (!pGetCodeVersionFromDevice || !m_pProcReadFromASIC || !m_pProcWriteToASIC)
#else
	if (!pGetCodeVersionFromDevice )
#endif
	{
		MessageBox(0, L"Load library function fail!", L"Error", MB_OK);
		if (hLibrary) FreeLibrary(hLibrary);
		hLibrary = NULL;
		fwCheckRes = 0;
	}

	GdiplusStartup(&gdiPlusToken, &gdiPlusStartupInput, NULL);

	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_CPPGDIPLUS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CPPGDIPLUS));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	Gdiplus::GdiplusShutdown(gdiPlusToken);
	return (int)msg.wParam;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;//  LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CPPGDIPLUS));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;// MAKEINTRESOURCE(IDC_CPPGDIPLUS);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindowEx(WS_EX_TRANSPARENT | WS_EX_LAYERED, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}
	/*
#define IDC_MAIN_BUTTON 101
	CreateWindowEx(NULL,
		L"BUTTON",
		L"OK",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		50,
		220,
		100,
		24,
		hWnd,
		(HMENU)IDC_MAIN_BUTTON,
		GetModuleHandle(NULL),
		NULL);
		*/

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}



bool HexCStringToDec(const CString &rszHex, int &riDec)
{
	CString szHex = rszHex;
	szHex.MakeLower();
	int iHexLength = szHex.GetLength();

	if (iHexLength == 0) return false;

	riDec = 0;
	for (int i = 0; i < iHexLength; i++)
	{
		wchar_t cVal = szHex.GetAt(i);
		int iVal;
		if ((cVal >= '0') && (cVal <= '9'))
			iVal = (int)(cVal - '0');
		else if ((cVal >= 'a') && (cVal <= 'f'))
			iVal = (int)(cVal - 'a' + 10);
		else if (cVal == ' ')
		{
			if (riDec != 0)
				return true;
			else
				continue;
		}
		else
			return false;
		riDec *= 16;
		riDec += iVal;
	}
	return true;
}

#ifdef FOCUS_CHECK
int GetAFRPT(void)
{
	// AF Report
	unsigned long ulAF_Win0_Rpt = 0, ulAF_Win1_Rpt = 0;

	int nAddress, nData, AFData;
	CString strASICData;

	focusCheckRes = 0;

	
	HexCStringToDec("08AE", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win0_Rpt = nData;
	ulAF_Win0_Rpt <<= 8;
	strASICData.Format(_T("%02x"), nData);
	memcpy(afReg3, strASICData, 5);

	if (nData > 0)
		focusRetry = 0;
	 //focusCheckRes = 1;
		
	HexCStringToDec("08AF", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win0_Rpt += nData;
	ulAF_Win0_Rpt <<= 8;
	strASICData.Format(_T("%02x"), nData);
	memcpy(afReg2, strASICData, 5);

	
	
	if(nData > 0)
	 focusCheckRes = 1;
		


	HexCStringToDec("08B0", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win0_Rpt += nData;
	ulAF_Win0_Rpt <<= 8;
	strASICData.Format(_T("%02x"), nData);
	memcpy(afReg1, strASICData, 5);
	/*
	if (nData > 0xa0)
		focusCheckRes = 1;
	*/
	if (nData > 0x50)
		focusCheckRes = 1;

	HexCStringToDec("08B1", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win0_Rpt += nData;
	strASICData.Format(_T("%02x"), nData);
	memcpy(afReg0, strASICData, 5);



	// Window 1

	HexCStringToDec("08B2", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win1_Rpt = nData;
	ulAF_Win1_Rpt <<= 8;
	strASICData.Format(_T("%02x"), nData);
	memcpy(af2Reg3, strASICData, 5);


	//ulAF_Win1_Rpt += aAF_W_SUM2;
	HexCStringToDec("08B3", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win1_Rpt += nData;
	ulAF_Win1_Rpt <<= 8;
	strASICData.Format(_T("%02x"), nData);
	memcpy(af2Reg2, strASICData, 5);


	//ulAF_Win1_Rpt += aAF_W_SUM1;
	HexCStringToDec("08B4", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win1_Rpt += nData;
	ulAF_Win1_Rpt <<= 8;
	strASICData.Format(_T("%02x"), nData);
	memcpy(af2Reg1, strASICData, 5);


	//ulAF_Win1_Rpt += aAF_W_SUM0;
	HexCStringToDec("08B5", nAddress);
	if (!(*m_pProcReadFromASIC)(nAddress, (BYTE*)&nData))
		return false;
	nData &= 0xff;
	ulAF_Win1_Rpt += nData;
	strASICData.Format(_T("%02x"), nData);
	memcpy(af2Reg0, strASICData, 5);


	strASICData.Format(_T("%x"), ulAF_Win0_Rpt);
	memcpy(afValueStr, strASICData, 20);

	strASICData.Format(_T("%x"), ulAF_Win1_Rpt);
	memcpy(afValueStr2, strASICData, 20);

	return 0;
}
#endif

#ifdef CURRENT_CHECK


int GetCurrentDataFromSerial(void)
{
	DWORD	dwByte;
	char	szRecv[20] = { 0 };
	int		nRet;
	char    szSend[10] = { "E\r\n" };
	DWORD	nByte;
	RecvValue = 0;

	if (hFile == INVALID_HANDLE_VALUE)
	{
		OutputDebugString(L"INVALID_HANDLE_VALUE\n");
		return -1;
	}
	
	nRet = WriteFile(hFile, szSend, 3, &nByte, 0);
	if (!nRet) 
	{
		OutputDebugString(L"WriteFile error\n");
		return -1;
	}

	memset(szRecv, 0, 20);
	nRet = ReadFile(hFile, szRecv, 20, &dwByte, NULL);
	if (!nRet) 
	{
		OutputDebugString(L"ReadFile error\n");
		return -1;
	}

	if (hFile != INVALID_HANDLE_VALUE)
	{
//		OutputDebugString(L"ClearCommError\n");
		DWORD DErr;
		ClearCommError(hFile, &DErr, NULL);
		PurgeComm(hFile, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	}

	if (dwByte > 0)
	{
		for (int idx = 0; idx < 10; idx++) {
			if (szRecv[idx] == 13) {
				RecvValue = (int)strtol(szRecv + idx + 1, NULL, 16);
				
				break;
			}
		}
	}
				
	//IMSLog(L"RecvValue = %d\n", RecvValue);
	if (RecvValue >= currentMinValue && RecvValue <= currentMaxValue) 
	{
		return 1;
	}
	
	return 0;
}
#endif



int firstError = 0;
int mipiCheck(void)
{
	CString strASICData;
	int nAddress, nData;

	//OutputDebugString(L"mipiCheck 1\n");
	HexCStringToDec("10f1", nAddress);
	//OutputDebugString(L"mipiCheck 2\n");
	
	{
		//OutputDebugString(L"mipiCheck 3\n");
	//	Sleep(100);
		if (no_device)
		{
			// Un-initial device
			(*m_pUnInitialize_DriverInterface)();
			return 0;
		}

		if (!(*m_pProcReadFromASIC)((USHORT)nAddress, (BYTE*)&nData)) // 이 부분에서 USB 가 빠지면 죽음
		{
			no_device = 1; //no device
			OutputDebugString(L"init device error\n");
			// Un-initial device
		//	(*m_pUnInitialize_DriverInterface)();
			//memcpy(mipiValueStr, "init fail", 20);
			return 0;
		}
		nData &= 0xff;

		IMSLog(L"MIPI = %02x\n", nData);
		
#ifdef CLEAR_MIPI_ERROR_REGISTER
		//if ((nData == 64) /*&& (firstError < 2)*/) //0x40 error
		if ((nData != 0) && (firstError == 0))
		{
		//	OutputDebugString(L"mipiCheck 5\n");
			// Write register
			(*m_pProcWriteToASIC)(nAddress, nData);

			firstError = 1;
			if (!(*m_pProcReadFromASIC)((USHORT)nAddress, (BYTE*)&nData)) // 이 부분에서 USB 가 빠지면 죽음
			{
				no_device = 1; //no device
				return 0;
			}

		}

		/*if (nData != 0)
		firstError++;*/
#endif

		//OutputDebugString(L"mipiCheck 4\n");
		

		no_device = 0; // device

		nData &= 0xff;

		strASICData.Format(_T("%02x"), nData);

		memcpy(mipiValueStr, strASICData, 20);

	//	IMSLog(L"MIPI = %s\n", mipiValueStr);

		if ((nData == 64) || (nData == 0))
		{
			mipiCheckRes = 1; //Mipi OK

		}
		else
		{
			mipiCheckRes = 0; //Mipi error
		}
	}
	return 0;
}

int checkFW(void)
{


	if (pGetCodeVersionFromDevice == NULL)
		return false;

	// Get FW code version
	memset(pbyCodeVersion, 0x00, DF_CODE_VERSION_SIZE);

	if (!(*pGetCodeVersionFromDevice)(pbyCodeVersion))
	{
		no_device = 1; //no device
		OutputDebugString(L"read version error\n");
		return false;
	}
	else
	{
		no_device = 0; //device
		OutputDebugString(L"read version OK\n");

		char tmpFwCode[DF_CODE_VERSION_SIZE] = { 0 };
		memcpy(tmpFwCode, pbyCodeVersion, DF_CODE_VERSION_SIZE);

		memset(fwVersionW, 0x00, sizeof(WCHAR));
		mbstowcs(fwVersionW, tmpFwCode, strlen(tmpFwCode));

		if (!strcmp(tmpFwCode, fwVersion)) {
			fwCheckRes = 1;
			OutputDebugString(L"Code Vesrion Pass\n");
		}
		else {
			fwCheckRes = 0;
			OutputDebugString(L"Code Vesrion Fail\n");
		}

    }


	return true;
}


#define MESSAGE_WINDOW_H 600/*342*/
#define MESSAGE_WINDOW_W 410/*402*/

int sendPaintMsg(void)
{
	RECT rect = { fwTextX, fwTextY, fwTextX + MESSAGE_WINDOW_W, fwTextY + MESSAGE_WINDOW_H };
	InvalidateRect(hWndCopy, &rect, TRUE);
	SendMessage(hWndCopy, WM_PAINT, 0, 0);

	return 0;
}


//#define ONESHOT_CHECK
int RecvDataSema = 0;

DWORD RecvData(VOID * dummy)
{
	if (RecvDataSema == -1)
		return -1;
	
	RecvDataSema = -1;

	Sleep(100);
	// Initial device		
	if (m_pInitialize_DriverInterface != NULL)
		(*m_pInitialize_DriverInterface)(0);

		
	OutputDebugString(L"RecvData Alive\n");

#ifdef ONESHOT_CHECK
	if (checkFW() == true)
	{

		fwTestDone = 1;
		
		OutputDebugString(L"fwTestDone\n");

		for (int i = 0; i < CURRENT_CHECK_RETRY; i++)
		{
			currentCheckRes = GetCurrentDataFromSerial();

			currentTestDone = 1;
			
			OutputDebugString(L"currentTestDone\n");

			if (currentCheckRes == -1)
				break;
			Sleep(10);
		}

		//	if (RecvData > 0) // very important
		{
			for (int i = 0; i < MIPI_CHECK_RETRY; i++)
			{
				mipiCheck();
				mipiTestDone = 1;
			
				OutputDebugString(L"mipiTestDone\n");
				Sleep(50);
			}
#ifdef FOCUS_CHECK
			GetAFRPT();
			focusTestDone = 1;
			sendPaintMsg();
			OutputDebugString(L"focusTestDone\n");
#endif
		}
		allTestDone = 1;
		
	}


	// Un-initial device
	if (m_pInitialize_DriverInterface != NULL)
		(*m_pUnInitialize_DriverInterface)();

	allTestDone = 1;
	sendPaintMsg();
/*
	fwTestDone = 0;
	currentTestDone = 0;
	mipiTestDone = 0;
	focusTestDone = 0;
	allTestDone = 0;
	*/
	Sleep(100);

#else
			
	if (checkFW() == true)
	{
#ifndef FOCUS
		fwTestDone = 1;
		sendPaintMsg();
		OutputDebugString(L"fwTestDone\n");
#endif

#ifdef MIPI_JIG
		for (int i = 0; i < 20; i++)
		{
			
				Sleep(100);
			
		}

#endif

#ifdef CURRENT_CHECK
		for (int i = 0; i < CURRENT_CHECK_RETRY ; i++)
		{
			currentCheckRes = GetCurrentDataFromSerial();
				
			currentTestDone = 1;
			
			//if ((currentCheckRes == -1) || (currentCheckRes == 1))
			if ((currentCheckRes == 1) && (i >= 2) ) //2017.05.23 최초 데이터는 이전카메라 값임
				break;
			if (currentCheckRes == -1)
			Sleep(100);
			sendPaintMsg();
		}
		
		OutputDebugString(L"currentTestDone\n");


		if (RecvData > 0) // very important : 전류를 체크할때는 전류가 0, -1 이면 미피 체크를 하지 않음
#endif
		{
			firstError = 0;
#ifdef MIPI_CHECK
#ifdef MIPI_CHECK_FOREVER
			while (1)
#else
			for (int i = 0; i < MIPI_CHECK_RETRY; i++)
#endif
			{
				if (no_device)
					break;
				mipiCheck();
				mipiTestDone = 1;
				
				Sleep(50);
#ifdef MIPI_CHECK_FOREVER
				allTestDone = 0;
				sendPaintMsg();
			};
#else
				
			}
			sendPaintMsg();
			OutputDebugString(L"mipiTestDone\n");
#endif
#endif
#ifdef FOCUS_CHECK
			for (focusRetry = 0; focusRetry < FOCUS_CHECK_RETRY; focusRetry++)
			//while(1)
			{
				if (!no_device)
				{
					Sleep(100);
					GetAFRPT();
					focusTestDone = 1;
					sendPaintMsg();
				//	OutputDebugString(L"focusTestDone\n");

					/*if (focusCheckRes)
						break;*/
				}
			}
#endif
		}
	
	}
		
		
	// Un-initial device
	if (m_pInitialize_DriverInterface != NULL)
		(*m_pUnInitialize_DriverInterface)();

	allTestDone = 1;
	sendPaintMsg();
		
	fwTestDone = 0;
	currentTestDone = 0;
	mipiTestDone = 0;
	focusTestDone = 0;
	allTestDone = 0;

	Sleep(100);
		
#endif


	OutputDebugString(L"Exit RecvData \n");
	RecvDataSema = 0;

	return 0;
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	WCHAR comPort[10] = { 0 };
	char szBuffer[200] = { 0 };

	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	RECT windowsize;    // get the height and width of the screen

	HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	monitor_width = info.rcMonitor.right - info.rcMonitor.left;
	monitor_height = info.rcMonitor.bottom - info.rcMonitor.top;


	switch (message)
	{
	case WM_CREATE:
	{

		RegisterForDeviceNotification(hWnd);
		//GetSymbolicLink();

		GetCurrentDirectory(MAX_PATH, currentDirectory);
		SetWindowLong(hWnd, GWL_STYLE, 0); // Remove Window Title Bar

		int exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
		//exStyle |= WS_EX_LAYERED;
		exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;

		SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);
		SetLayeredWindowAttributes(hWnd, RGB(10, 10, 10), 128, LWA_ALPHA | LWA_COLORKEY);
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, monitor_width, monitor_height, SWP_ASYNCWINDOWPOS);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// INI File Loading

		TCHAR iniFilePath[MAX_PATH] = { 0 };
		wsprintf(iniFilePath, L"%s\\IMSGRID.ini", currentDirectory);

		if (!FileExists(iniFilePath)) {

			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"COM_PORT", L"COM1", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"CURRENT_MIN", L"100", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"CURRENT_MAX", L"300", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"CURRENT_OFFSET", L"0", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"FIRMWARE_VERSION", L"SN9C27103.00.18-6B2-53", iniFilePath);
			//WritePrivateProfileString(L"DEFAULT_SETTINGS", L"FIRMWARE_CHECK_INTERVAL", L"250", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_GRID", L"1", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_GRID_BOX", L"1", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_TEXT_X", L"1000", iniFilePath);
			WritePrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_TEXT_Y", L"330", iniFilePath);

			WCHAR tmpMessage[MAX_PATH + 100] = { 0 };
			wsprintf(tmpMessage, L"%s open fail! Default ini file created.", iniFilePath);
			MessageBox(0, tmpMessage, L"Error", MB_OK);

		}
		DWORD result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"COM_PORT", L"COM1", comPort, 256, iniFilePath);

		TCHAR tmpValue[256] = { 0 };

		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"CURRENT_MIN", L"100", tmpValue, 256, iniFilePath);
		currentMinValue = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"CURRENT_MAX", L"300", tmpValue, 256, iniFilePath);
		currentMaxValue = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"CURRENT_OFFSET", L"-10", tmpValue, 256, iniFilePath);
		OffsetValue = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"FIRMWARE_VERSION", L"SN9C27103.00.18-6B2-53", tmpValue, 256, iniFilePath);
		WideCharToMultiByte(CP_ACP, 0, tmpValue, -1, fwVersion, wcslen(tmpValue), 0, 0);

		//memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		//result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"FIRMWARE_CHECK_INTERVAL", L"250", tmpValue, 256, iniFilePath);
		//fwCheckInterval = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_GRID", L"1", tmpValue, 256, iniFilePath);
		drawGrid = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_GRID_BOX", L"1", tmpValue, 256, iniFilePath);
		drawGridBox = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_TEXT_X", L"100", tmpValue, 256, iniFilePath);
		fwTextX = _wtoi(tmpValue);

		memset(tmpValue, 0x00, sizeof(TCHAR) * 256);
		result = GetPrivateProfileString(L"DEFAULT_SETTINGS", L"DRAW_TEXT_Y", L"100", tmpValue, 256, iniFilePath);
		fwTextY = _wtoi(tmpValue);

		

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		hWndCopy = hWnd;
		nSwitch = 1;

		hFile = CreateFile(comPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			WCHAR tmpMessage[100] = { 0 };
			wsprintf(tmpMessage, L"%s Port open fail!", comPort);
#ifdef CURRENT_CHECK
			MessageBox(0, tmpMessage, L"Error", MB_OK);
#endif
			RecvValue = -1;
		}
		else
		{

			DCB	dcb;

			GetCommState(hFile, &dcb);

			int InBufSize = 1024;
			int OutBufSize = 1024;
			DWORD DErr;

			//ClearCommError(hFile, &DErr, NULL);

			//SetupComm(hFile, InBufSize, OutBufSize);
			//PurgeComm(hFile, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);


			dcb.BaudRate = CBR_115200; // CBR_38400;
			dcb.StopBits = ONESTOPBIT;
			dcb.Parity = NOPARITY;
			dcb.ByteSize = 8;

			SetCommState(hFile, &dcb);

			COMMTIMEOUTS	cTimeout;

			//GetCommTimeouts(hFile, &cTimeout);	

			cTimeout.ReadIntervalTimeout = 1000;
			cTimeout.ReadTotalTimeoutMultiplier = 0;
			cTimeout.ReadTotalTimeoutConstant = 1000;
			cTimeout.WriteTotalTimeoutMultiplier = 0;
			cTimeout.WriteTotalTimeoutConstant = 0;


			SetCommTimeouts(hFile, &cTimeout);

			/*Sleep(1000);
			char szSend[10] = { "E\r\n" };
			DWORD	nByte;
			if (!WriteFile(hFile, szSend, 3, &nByte, 0)) {
				RecvValue = -1;
			}*/

		
		}

//#ifndef FOCUS
		RecvData(0);
//#endif
		
/*
		DWORD nThreadId;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvData, NULL, 0, &nThreadId);
		
*/
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	}
	break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			GdiplusShutdown(gdiPlusToken);
			break;
			//case ID_EDIT_RECV:
			//	hdc = BeginPaint(hWnd, &ps);
			//	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, monitor_width, monitor_height, SWP_ASYNCWINDOWPOS);
			//	OnPaint(hdc);
			//	EndPaint(hWnd, &ps);
			//	break;
			//case ID_EDIT_SEND:
			//	// �۽��� ����Ʈ ��Ʈ���� ��ȭ�� ��
			//	if (HIWORD(wParam) == EN_CHANGE) {

			//		char szSend[10];

			//		// �۽��� ������ ���ۿ��� �о����
			//		GetDlgItemText(hWnd, ID_EDIT_SEND, szBuffer, sizeof(szBuffer));

			//		// ���� ���� ��ġ ���
			//		int nPos = (int)SendDlgItemMessage(hWnd, ID_EDIT_SEND, EM_GETSEL, 0, 0);

			//		// ���� �׸� ���� ��ġ
			//		int nStart = LOWORD(nPos);
			//		szSend[0] = szBuffer[nStart - 1];

			//		DWORD	nByte;

			//		// �ѹ��� �۽�
			//		WriteFile(hFile, szSend, 1, &nByte, 0);
			//	}
			//	return TRUE;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		
		::SetWindowPos(hWnd,       // handle to window
			HWND_TOPMOST,  // placement-order handle
			0,     // horizontal position
			0,      // vertical position
			0,  // width
			0, // height
			SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE// window-positioning options
		);
		
		hdc = BeginPaint(hWnd, &ps);
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, monitor_width, monitor_height, SWP_ASYNCWINDOWPOS);
		//OnPaint(hdc);
#if 0
		if (firstDraw == 1)
		{
	        OnPaint_1(hdc);
			OnPaint_2(hdc);
			firstDraw = 0;
		}
		else
		{		
			OnPaint_2(hdc);
		}
#else

		OnPaint_1(hdc);
		
		OnPaint_2(hdc);
		
		
#endif
		EndPaint(hWnd, &ps);

		break;

	case WM_DESTROY:
		nSwitch = 0;
		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
		}
		if (hLibrary) FreeLibrary(hLibrary);
		hLibrary = NULL;
		PostQuitMessage(0);
		break;
	case WM_DEVICECHANGE:

		if (lParam != 0)
		{
			switch (wParam)
			{
			case DBT_DEVICEARRIVAL:
				//MessageBox(hWnd, L"arrival the capture device.", NULL, MB_OK);
				OutputDebugString(L"arrival the capture device.\n");
				no_device = 0;
				RecvData(0);
				break;

			case DBT_DEVICEREMOVECOMPLETE:
#ifdef MIPI_CHECK_FOREVER
				SendMessage(hWndCopy, WM_DESTROY, 0, 0);
#endif
				no_device = 1;
				sendPaintMsg();
				//MessageBox(hWnd, L"Lost the capture device.", NULL, MB_OK);
				OutputDebugString(L"lost the capture device.\n");
				break;

			
			}
		}
		


		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


void OnPaint_1(HDC hDC)
{
	Graphics g(hDC);

	/*TCHAR imagePath[MAX_PATH] = { 0 };
	wsprintf(imagePath, L"%s\\grid.png", currentDirectory);
	*/

	SolidBrush brush(Color(255, 10, 10, 10));
    //SolidBrush brush(Color(255, 0, 0, 0));
	g.FillRectangle(&brush, 0, 0, monitor_width, monitor_height);
#ifndef FOCUS
	/*Image image(imagePath);
	Status status = g.DrawImage(&image, 0, 0);
	if (status == Ok) { return; }
	*/

	Pen linePen(Color::White, 1.0);
	Pen linePen2(Color::Black, 3.0);

	Pen linePen3(Color::White, 2.0);
	Pen linePen4(Color::Black, 4.0);

	Pen linePen5(Color::Black, 3.0);
	Pen linePen6(Color::Black, 1.0);
	Pen linePen7(Color::White, 3.0);

	Pen linePen8(Color::Blue, 3.0);
	Pen linePen9(Color::Red, 3.0);

	int x = 0;
	int y = 0;
	int offset_x = 0;
	int offset_y = 18;

	int step = 36;

	int step_2 = (int)((float)step * 0.25f);
	int step_8 = (int)((float)step * 0.85f);


	if (drawGrid) 
	{

		
		g.DrawLine(&linePen7, monitor_width / 2, 0, monitor_width / 2, monitor_height);
		g.DrawLine(&linePen7, 0, monitor_height / 2, monitor_width, monitor_height / 2);

		g.DrawLine(&linePen6, monitor_width / 2, 0, monitor_width / 2, monitor_height);
		g.DrawLine(&linePen6, 0, monitor_height / 2, monitor_width, monitor_height / 2);


		//g.DrawEllipse(&linePen5, (monitor_width / 2) - step, (monitor_height / 2) - step, step * 2, step * 2);

		//g.DrawString(L"2", -1, &font, PointF((monitor_width / 2) + 2, (monitor_height / 2) + 8), &strBrush);
		//g.DrawString(L"8", -1, &font, PointF((monitor_width / 2) + 31, (monitor_height / 2) + 8), &strBrush);
		//g.DrawString(L"-2", -1, &font, PointF((monitor_width / 2) - 22, (monitor_height / 2) + 8), &strBrush);
		//g.DrawString(L"-8", -1, &font, PointF((monitor_width / 2) - 49, (monitor_height / 2) + 8), &strBrush);


		int x1 = (monitor_width / 2) - (step * 18);
		int x2 = (monitor_width / 2) + (step * 17);
		int y1 = (monitor_height / 2) - (step * 10);
		int y2 = (monitor_height / 2) + (step * 9);

		g.DrawRectangle(&linePen7, x1, y1, step, step);
		g.DrawRectangle(&linePen7, x2, y2, step, step);
		g.DrawRectangle(&linePen7, x2, y1, step, step);
		g.DrawRectangle(&linePen7, x1, y2, step, step);

		g.DrawRectangle(&linePen6, x1, y1, step, step);
		g.DrawRectangle(&linePen6, x2, y2, step, step);
		g.DrawRectangle(&linePen6, x2, y1, step, step);
		g.DrawRectangle(&linePen6, x1, y2, step, step);


		g.DrawLine(&linePen7, x1, y1 + (step / 2), x1 + step, y1 + (step / 2));
		g.DrawLine(&linePen7, x2, y1 + (step / 2), x2 + step, y1 + (step / 2));
		g.DrawLine(&linePen7, x1, y2 + (step / 2), x1 + step, y2 + (step / 2));
		g.DrawLine(&linePen7, x2, y2 + (step / 2), x2 + step, y2 + (step / 2));

		g.DrawLine(&linePen7, x1 + (step / 2), y1, x1 + (step / 2), y1 + step);
		g.DrawLine(&linePen7, x2 + (step / 2), y1, x2 + (step / 2), y1 + step);
		g.DrawLine(&linePen7, x1 + (step / 2), y2, x1 + (step / 2), y2 + step);
		g.DrawLine(&linePen7, x2 + (step / 2), y2, x2 + (step / 2), y2 + step);


		g.DrawLine(&linePen6, x1, y1 + (step / 2), x1 + step, y1 + (step / 2));
		g.DrawLine(&linePen6, x2, y1 + (step / 2), x2 + step, y1 + (step / 2));
		g.DrawLine(&linePen6, x1, y2 + (step / 2), x1 + step, y2 + (step / 2));	g.DrawLine(&linePen6, x2, y2 + (step / 2), x2 + step, y2 + (step / 2));

		g.DrawLine(&linePen6, x1 + (step / 2), y1, x1 + (step / 2), y1 + step);
		g.DrawLine(&linePen6, x2 + (step / 2), y1, x2 + (step / 2), y1 + step);
		g.DrawLine(&linePen6, x1 + (step / 2), y2, x1 + (step / 2), y2 + step);
		g.DrawLine(&linePen6, x2 + (step / 2), y2, x2 + (step / 2), y2 + step);

		if (drawGridBox) {
			g.DrawRectangle(&linePen8, (monitor_width / 2) - step_2, (monitor_height / 2) - step_2, step_2 * 2, step_2 * 2);
			//g.DrawRectangle(&linePen9, (monitor_width / 2) - step_8, (monitor_height / 2) - step_8 , step_8 * 2, step_8 * 2 ); //정사각형, 8mm x 8mm
			//g.DrawRectangle(&linePen9, (monitor_width / 2) - step_8, (monitor_height / 2) - step_8 - 4, step_8 * 2, step_8 * 2 + 8); //8mm x 10mm (상 2mm, 하 2mm 늘림)
			g.DrawRectangle(&linePen9, (monitor_width / 2) - step_8, (monitor_height / 2) - step_8 - 8, step_8 * 2, step_8 * 2 + 16); //8mm x 12mm (상 4mm, 하 4mm 늘림)
		}
		else {
			g.DrawEllipse(&linePen8, (monitor_width / 2) - step_2, (monitor_height / 2) - step_2, step_2 * 2, step_2 * 2);
			g.DrawEllipse(&linePen9, (monitor_width / 2) - step_8, (monitor_height / 2) - step_8, step_8 * 2, step_8 * 2);
		}
		g.DrawRectangle(&linePen9, (monitor_width / 3 * 2) , (monitor_height / 3) , step_8 * 2, step_8 * 2);

	}
#endif
}

#define FONT_SIZE 80
#define FONT2_SIZE 30

void OnPaint_2(HDC hDC)
{
	Pen linePen6(Color::Black, 1.0);
	Pen linePen7(Color::White, 3.0);
	Graphics g(hDC);
	Font font(L"Arial", FONT_SIZE, FontStyleBold, UnitPixel);
	Font font2(L"Arial", FONT2_SIZE, FontStyleBold, UnitPixel);

	SolidBrush brush(Color(255, 10, 10, 10));
	//SolidBrush brush(Color(255, 0, 0, 0));

	SolidBrush strBrush1(Color(255, 0, 0, 0));
	SolidBrush strBrush2(Color(255, 255, 0, 0));
	SolidBrush strBrush3(Color(255, 255, 255, 255));
	SolidBrush strBrush4(Color(255, 0, 125, 255));

	int textX = fwTextX;// (monitor_width / 2);
	int textX2 = fwTextX + 16;// (monitor_width / 2);

	int textY = fwTextY;// (monitor_height / 2);
	int textY2 = textY + FONT_SIZE;// (monitor_height / 2);
	int textY3 = textY2 + FONT2_SIZE;// (monitor_height / 2);
	int textY4 = textY3 + FONT_SIZE;
	int textY5 = textY4 + FONT2_SIZE;
	int textY6 = textY5 + FONT_SIZE;

	int textY7 = textY6 + FONT2_SIZE;
	int textY8 = textY7 + FONT_SIZE;
	int textY9 = textY8 + FONT2_SIZE;

	int textY10 = textY9 + FONT2_SIZE;
	int textY11 = textY10 + FONT2_SIZE;



	WCHAR fwCheckStr[20] = { 0 };
	WCHAR mipiCheckStr[20] = { 0 };
	WCHAR afCheckStr[20] = { 0 };
	TCHAR valueString[20] = { 0 };
	TCHAR checkString[20] = { 0 };
	TCHAR msgString[20] = { 0 };
	TCHAR msgString2[20] = { 0 };
	BOOL currentFail = false;
	SolidBrush *fwBrush = &strBrush2;
	
	g.FillRectangle(&brush, fwTextX, fwTextY, MESSAGE_WINDOW_W, MESSAGE_WINDOW_H);
	
	if (no_device)
	{
		wcscpy(msgString, L"카메라를 연결하세요");
		g.DrawString(msgString, -1, &font2, PointF(textX , textY ), fwBrush);
#ifndef FOCUS
		g.DrawLine(&linePen6, monitor_width / 2, 0, monitor_width / 2, monitor_height);
		g.DrawLine(&linePen6, 0, monitor_height / 2, monitor_width, monitor_height / 2);
#endif

		return;
	}
#ifndef FOCUS
	//FW verion check
	if (fwTestDone)
	{
		if (fwCheckRes == 1) {
			wcscpy(fwCheckStr, L"버젼 OK");
			fwBrush = &strBrush4;
		}
		else if (fwCheckRes == 0 && wcslen(fwVersionW) > 0) {
			wcscpy(fwCheckStr, L"버젼 에러");
			fwBrush = &strBrush2;
		}
		else if (wcslen(fwVersionW) <= 0) {
			wcscpy(fwCheckStr, L"");
		}
		else {
			memset(valueString, 0x00, sizeof(TCHAR) * 20);
			memset(checkString, 0x00, sizeof(TCHAR) * 20);

		}
		g.FillRectangle(&brush, textX, textY, MESSAGE_WINDOW_W, FONT_SIZE + FONT2_SIZE);
		g.DrawString(fwCheckStr, -1, &font, PointF(textX , textY ), fwBrush);
		g.DrawString(fwVersionW, -1, &font2, PointF(textX2 , textY2 ), fwBrush);
		//fwTestDone = 0;
	}
#endif

#ifdef	CURRENT_CHECK
	//Current check
	if (currentTestDone)
	{
		
#if 1
		if (RecvValue >= 0)
		//if (currentCheckRes >= 0) 
		{
			wsprintf(valueString, L"%d mA", RecvValue);
			if (RecvValue >= currentMinValue && RecvValue <= currentMaxValue) {
				wsprintf(checkString, L"전류 OK");
				currentFail = false;
			}
			else if (RecvValue < currentMinValue)
			{
				wsprintf(checkString, L"전류 낮음");
				currentFail = true;
			}
			else if (RecvValue > currentMaxValue)
			{
				wsprintf(checkString, L"전류 높음");
				currentFail = true;
			}
			
		}
		else 
		{
			
			wsprintf(checkString, L"COM포트");
			wsprintf(valueString, L"를 확인하세요");
			
			currentFail = true;
		}


		if (!currentFail) {
			fwBrush = &strBrush4;
		}
		else
			fwBrush = &strBrush2;
#else
		if (currentCheckRes == 1 ) {
			wsprintf(checkString, L"전류 OK");
			wsprintf(valueString, L"%d mA", RecvValue);
			fwBrush = &strBrush4;
		}
		else if (currentCheckRes == 0) {
			wsprintf(checkString, L"전류 에러");
			wsprintf(valueString, L"%d mA", RecvValue);
			fwBrush = &strBrush2;
		}
		else {
			wsprintf(checkString, L"시리얼");
			wsprintf(valueString, L"에러");
			fwBrush = &strBrush2;
		}
#endif	
		g.FillRectangle(&brush, textX, textY3, MESSAGE_WINDOW_W, FONT_SIZE+FONT2_SIZE);
	
		g.DrawString(checkString, -1, &font, PointF(textX , textY3 ), fwBrush);
		g.DrawString(valueString, -1, &font2, PointF(textX2, textY4 ), fwBrush);
		//currentTestDone = 0;
	}
#endif

	if (mipiTestDone)
	{
		//Mipi check
		if (mipiCheckRes == 1) {
			wcscpy(mipiCheckStr, L"MIPI OK");
			fwBrush = &strBrush4;
		}
		else if (mipiCheckRes == 0) {
			wcscpy(mipiCheckStr, L"MIPI Error");
			fwBrush = &strBrush2;
		}

		g.FillRectangle(&brush, textX, textY5, MESSAGE_WINDOW_W, FONT_SIZE + FONT2_SIZE);
		g.DrawString(mipiCheckStr, -1, &font, PointF(textX, textY5 ), fwBrush);
		g.DrawString(mipiValueStr, -1, &font2, PointF(textX2, textY6), fwBrush);
		//mipiTestDone = 0;
	}
#ifdef FOCUS_CHECK
	//Focus check
	if (focusTestDone)
	{
		/*if (focusCheckRes == 1) {
			wcscpy(afCheckStr, L"Focus OK");
			fwBrush = &strBrush4;
		}
		else if (focusCheckRes == 0) {
		//	wcscpy(afCheckStr, L"촛점 에러");
			wcscpy(afCheckStr, L"Focus fail");
			fwBrush = &strBrush2;
		}
		*/
		g.FillRectangle(&brush, textX, textY7, MESSAGE_WINDOW_W, FONT_SIZE + FONT2_SIZE + FONT2_SIZE);
		//g.DrawString(afCheckStr, -1, &font, PointF(textX, textY7), fwBrush);

#if 0
		g.DrawString(afValueStr, -1, &font2, PointF(textX2 , textY8), fwBrush);
		g.DrawString(afValueStr2, -1, &font2, PointF(textX2 , textY9), fwBrush);
#else
		g.DrawString(afReg3, -1, &font2, PointF(textX2, textY8), fwBrush);
		g.DrawString(afReg2, -1, &font2, PointF(textX2 + 40, textY8), fwBrush);
		g.DrawString(afReg1, -1, &font2, PointF(textX2 + 80, textY8), fwBrush);
		g.DrawString(afReg0, -1, &font2, PointF(textX2 + 120, textY8), fwBrush);

		g.DrawString(af2Reg3, -1, &font2, PointF(textX2, textY9), fwBrush);
		g.DrawString(af2Reg2, -1, &font2, PointF(textX2 + 40, textY9), fwBrush);
		g.DrawString(af2Reg1, -1, &font2, PointF(textX2 + 80, textY9), fwBrush);
		g.DrawString(af2Reg0, -1, &font2, PointF(textX2 + 120, textY9), fwBrush);
#endif
		//focusTestDone = 0;
	}
#endif
	if (allTestDone == 1)
	{
#ifdef FOCUS
		if (focusCheckRes == 1) {
			wcscpy(afCheckStr, L"Focus OK");
			fwBrush = &strBrush4;
		}
		else if (focusCheckRes == 0) {
			//	wcscpy(afCheckStr, L"촛점 에러");
			wcscpy(afCheckStr, L"Focus fail");
			fwBrush = &strBrush2;
		}

		g.DrawString(afCheckStr, -1, &font, PointF(textX, textY7), fwBrush);
#endif
		wcscpy(msgString, L"테스트가 끝났습니다");
		wcscpy(msgString2, L"카메라를 분리하세요");
		fwBrush = &strBrush2;
		g.DrawString(msgString, -1, &font2, PointF(textX2, textY10), fwBrush);
		g.DrawString(msgString2, -1, &font2, PointF(textX2, textY11), fwBrush);
		//allTestDone = 0;
	}

}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
