#ifndef _NETLOGODLG_CPP
#define _NETLOGODLG_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <StdIO.H>

#include "../Source/Entry.H"
#include "../Source/Routines.H"
#include "../Resources/Resource.H"
#include "../NSWFL/NSWFL.H"

#include "NetLogoDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::Windows;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HWND NetLogoDialog_hWnd = NULL;
HINSTANCE hModuleHandle = NULL;
LPABOUTDLGINFO glpADI;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool NetLogo(LPABOUTDLGINFO lpADI)
{
	HWND OwnerHandle = NULL;

	glpADI = lpADI;

	if (glpADI->OwnerHandle == NULL)
		OwnerHandle = GetActiveWindow();
	else OwnerHandle = glpADI->OwnerHandle;

	hModuleHandle = GetModuleHandle(NULL);

	DialogBox(hModuleHandle, MAKEINTRESOURCE(IDD_NETLOGO), OwnerHandle, (DLGPROC)NetLogoDialog);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HWND NetLogoHandle(void)
{
	return NetLogoDialog_hWnd;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool IsNetLogoActive(void)
{
	return (NetLogoDialog_hWnd != NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void EndNetLogo(void)
{
	EndDialog(NetLogoDialog_hWnd, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK NetLogoDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//--------------------------------------------------------------------------

	static HWND AppName_hWnd = NULL;
	static HWND AppVersion_hWnd = NULL;
	static HWND AppCopyRight_hWnd = NULL;
	static HWND DisplayIcon_hWnd = NULL;
	static HWND BuildTimeDate_hWnd = NULL;
	static HWND NetLink_hWnd = NULL;

	static HCURSOR HandCursor = NULL;

	static DWORD BGColorRef = 0;

	//--------------------------------------------------------------------------

	if (uMsg == WM_INITDIALOG) // Received A Initilize Dialog Message
	{
		char sTempText[255];

		NetLogoDialog_hWnd = hWnd;

		HandCursor = LoadCursor(hModuleHandle, MAKEINTRESOURCE(IDC_HANDCURSOR));
		BGColorRef = GetSysColor(COLOR_3DFACE);

		sprintf_s(sTempText, sizeof(sTempText), "NetworkDLS :: %s", glpADI->TitleCaption);
		SendMessage(hWnd, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)sTempText);

		AppName_hWnd = GetDlgItem(hWnd, IDC_APPNAME);
		AppVersion_hWnd = GetDlgItem(hWnd, IDC_APPVERSION);
		BuildTimeDate_hWnd = GetDlgItem(hWnd, IDC_BUILDTIMEDATE);
		AppCopyRight_hWnd = GetDlgItem(hWnd, IDC_APPCOPYRIGHT);
		DisplayIcon_hWnd = GetDlgItem(hWnd, IDC_ABOUTICON);
		NetLink_hWnd = GetDlgItem(hWnd, IDC_NETLINK);

		SendMessage(DisplayIcon_hWnd, (UINT)STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)glpADI->DisplayIcon);

		Set_Text(AppName_hWnd, glpADI->TitleCaption);

		Set_Text(AppVersion_hWnd, glpADI->FileVersion);

		sprintf_s(sTempText, sizeof(sTempText), "%s - %s", glpADI->BuildDate, glpADI->BuildTime);
		Set_Text(BuildTimeDate_hWnd, sTempText);

		Set_Text(AppCopyRight_hWnd, glpADI->CopyRight);

		CenterOverOwner(hWnd);

		return TRUE; // Return TRUE to set the keyboard focus, Otherwise return FALSE
	}

	//--------------------------------------------------------------------------

	if (IsMouseOverHwnd(NetLink_hWnd))
	{
		SetCursor(HandCursor);

		if (uMsg == WM_LBUTTONUP)
		{
			ShellExecute(0, "Open", "Http://www.NetworkDLS.com/", NULL, NULL, SW_SHOWNORMAL);
		}
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_CTLCOLORSTATIC)
	{
		if ((HANDLE)lParam == NetLink_hWnd)
			return Set_Color(RGB(0, 0, 255), BGColorRef, wParam);

		return FALSE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_COMMAND)
	{
		if (wParam == IDC_OKBUTTON)
		{
			EndDialog(hWnd, 0);
			DestroyWindow(hWnd);
			return TRUE;
		}
		return FALSE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_PAINT)
	{
		HDC ThisHDC;
		PAINTSTRUCT ThisPS;

		ThisHDC = BeginPaint(hWnd, &ThisPS);

		// Any painting should be done here

		EndPaint(hWnd, &ThisPS);
		return TRUE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_DESTROY)
	{
		FreeLibrary(hModuleHandle);
		hModuleHandle = NULL;
		NetLogoDialog_hWnd = NULL;
		return TRUE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_CLOSE)  // Received Close Message
	{
		EndDialog(hWnd, 0);
		DestroyWindow(hWnd);
		return TRUE;
	}

	//--------------------------------------------------------------------------

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
