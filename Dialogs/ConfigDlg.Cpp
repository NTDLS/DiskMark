#ifndef _ConfigDlg_CPP
#define _ConfigDlg_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>

#include "../Resources/Resource.H"

#include "../NSWFL/NSWFL.H"
#include "../CMathParser/CMathParser.h"

#include "../Source/Entry.H"
#include "../Source/Routines.H"

#include "../Dialogs/ConfigDlg.H"
#include "../Dialogs/MainDlg.H"
#include "../Dialogs/NetLogoDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::Windows;
using namespace NSWFL::File;
using namespace NSWFL::Registry;
using namespace NSWFL::String;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------(Variable Declarations)
HWND hConfigDialog = NULL;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PopDiskInformation(HWND hDiskDrive)
{
	int iPos = 65; //Drive A
	int iDriveType = 0;
	char sDrive[5];
	char sText[1024];

	while (iPos <= 90)
	{
		sprintf_s(sDrive, sizeof(sDrive), "%c:\\", iPos);

		if ((iDriveType = GetDriveType(sDrive)) != DRIVE_NO_ROOT_DIR)
		{
			sprintf_s(sText, sizeof(sText), "%s", sDrive);

			if (iDriveType == DRIVE_UNKNOWN) {
				strcat_s(sText, sizeof(sText), " (Unknown Media)");
			}
			else if (iDriveType == DRIVE_REMOVABLE) {
				strcat_s(sText, sizeof(sText), " (Removable Media)");
			}
			else if (iDriveType == DRIVE_FIXED) {
				strcat_s(sText, sizeof(sText), " (Fixed Disk)");
			}
			else if (iDriveType == DRIVE_REMOTE) {
				strcat_s(sText, sizeof(sText), " (Network Drive)");
			}
			else if (iDriveType == DRIVE_CDROM) {
				strcat_s(sText, sizeof(sText), " (CD-ROM Drive)");
			}
			else if (iDriveType == DRIVE_RAMDISK) {
				strcat_s(sText, sizeof(sText), " (RAM disk)");
			}

			if (iDriveType != DRIVE_UNKNOWN && iDriveType != DRIVE_CDROM)
			{
				SendMessage(hDiskDrive, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)sText);
			}
		}

		iPos++;
	}

	LRESULT iIndex = 0;

	if ((iIndex = SendMessage(hDiskDrive, (UINT)CB_FINDSTRING, (WPARAM)0, (LPARAM)"C:\\")) == CB_ERR)
	{
		iIndex = 0;
	}

	SendMessage(hDiskDrive, (UINT)CB_SETCURSEL, (WPARAM)iIndex, (LPARAM)0);

	SendMessage(hDiskDrive, EM_LIMITTEXT, 255, 0);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK ConfigDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//--------------------------------------------------------------------------

	static HWND hDiskDrive = NULL;
	static HWND hChunkSize = NULL;
	static HWND hIOS = NULL;
	static HWND hIterations = NULL;

	//--------------------------------------------------------------------------

	if (uMsg == WM_INITDIALOG) // Received A Initialize Dialog Message
	{
		hConfigDialog = hWnd;

		HMENU SystemMenu_hMenu = GetSystemMenu(hWnd, FALSE);
		AppendMenu(SystemMenu_hMenu, MF_SEPARATOR, 0, 0);
		AppendMenu(SystemMenu_hMenu, MF_STRING, CONFIGMENU_MENU_ABOUT, "About");

		char sCaption[128];
		sprintf_s(sCaption, sizeof(sCaption), "%s - Benchmark Settings", gsTitleCaption);
		SendMessage(hWnd, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)sCaption);
		SendMessage(hWnd, WM_SETICON, TRUE, (LPARAM)LoadIcon(ghAppInstance, MAKEINTRESOURCE(IDI_MAIN)));

		hDiskDrive = GetDlgItem(hWnd, IDC_DISKDRIVE);
		hChunkSize = GetDlgItem(hWnd, IDC_CHUNKSIZE);
		hIOS = GetDlgItem(hWnd, IDC_IOS);
		hIterations = GetDlgItem(hWnd, IDC_ITERATIONS);

		PopDiskInformation(hDiskDrive);

		SelectComboItem(hDiskDrive, gsDrive, 3);
		Set_Text(hChunkSize, gsLastChunkSize);
		Set_Text(hIOS, gsLastIOS);
		Set_Text(hIterations, gsLastIterations);

		if (gbUseWriteCache)
		{
			CheckDlgButton(hWnd, IDC_ALLOWCACHING, BST_CHECKED);
		}

		CenterWindow(hWnd);

		return TRUE; // Return TRUE to set the keyboard focus, Otherwise return FALSE.
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_COMMAND)
	{
		if (wParam == IDC_CMDHELP) //- Ok Button.
		{
			OpenHelp(hWnd);
			return TRUE;
		}

		if (wParam == IDC_CMDOK || wParam == IDOK) //- Ok Button.
		{
			char sText[1024];

			if (!Get_Text(hDiskDrive, gsDrive, sizeof(gsDrive)))
			{
				MessageBox(hWnd, "You must select a drive to test.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			if (!isalpha((unsigned char)gsDrive[0]))
			{
				MessageBox(hWnd, "You must select a drive to test.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			// Reduce "C:\ (Fixed Disk)" (or anything typed) to the drive root.
			gsDrive[1] = ':';
			gsDrive[2] = '\\';
			gsDrive[3] = '\0';

			CMathParser mathParser;

			gbUseWriteCache = (IsDlgButtonChecked(hWnd, IDC_ALLOWCACHING) == BST_CHECKED);

			struct { HWND hEdit; char* sText; int iTextSz; int* piValue; const char* sName; } fields[3] = {
				{ hChunkSize, gsLastChunkSize, (int)sizeof(gsLastChunkSize), &giBufferSize, "Block size" },
				{ hIOS, gsLastIOS, (int)sizeof(gsLastIOS), &giRounds, "Blocks per run" },
				{ hIterations, gsLastIterations, (int)sizeof(gsLastIterations), &giRuns, "Runs" }
			};

			for (int i = 0; i < 3; i++)
			{
				if (Get_Text(fields[i].hEdit, fields[i].sText, fields[i].iTextSz) <= 0)
				{
					sprintf_s(sText, sizeof(sText), "You must enter a value for \"%s\".", fields[i].sName);
					MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				if (mathParser.Calculate(fields[i].sText, fields[i].piValue) != CMathParser::ResultOk)
				{
					sprintf_s(sText, sizeof(sText), "An error occurred parsing \"%s\": %s", fields[i].sName, mathParser.LastError()->Text);
					MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}
			}

			MEMORYSTATUSEX ms;
			memset(&ms, 0, sizeof(ms));
			ms.dwLength = sizeof(ms);
			GlobalMemoryStatusEx(&ms);

			if (giBufferSize < 1)
			{
				MessageBox(hWnd, "The \"Block size\" must be greater than zero.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			else if ((unsigned __int64)giBufferSize > (ms.ullTotalPhys / 4))
			{
				char sSize[64];
				sprintf_s(sText, "The \"Block size\" cannot exceed 1/4th of physical RAM (%s)",
					FileSizeFriendly((__int64)(ms.ullTotalPhys / 4), sSize, sizeof(sSize)));
				MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			if (giRounds < 1 || giRounds > 100000)
			{
				MessageBox(hWnd, "\"Blocks per run\" must be between 1 and 100,000.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			if (giRuns < 1 || giRuns > 10000)
			{
				MessageBox(hWnd, "\"Runs\" must be between 1 and 10,000.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			// Reads are always unbuffered, so the block size must always be a sector multiple.
			int iSectorAligned = SectorAlign(gsDrive, giBufferSize);
			if (iSectorAligned <= 0)
			{
				MessageBox(hWnd, "Failed to determine the sector size of the selected volume.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			giBufferSize = iSectorAligned;

			char sVolumeNameBuffer[1024];
			DWORD dwFileSystemFlags;
			char sFileSystemNameBuffer[1024];

			if (GetVolumeInformation(gsDrive,
				sVolumeNameBuffer,
				sizeof(sVolumeNameBuffer),
				NULL,
				NULL,
				&dwFileSystemFlags,
				sFileSystemNameBuffer,
				sizeof(sFileSystemNameBuffer)))
			{
				if (dwFileSystemFlags & FILE_READ_ONLY_VOLUME)
				{
					MessageBox(hWnd, "The selected volume is read-only.", gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				ULARGE_INTEGER FreeBytesAvailable;
				if (GetDiskFreeSpaceEx(gsDrive, &FreeBytesAvailable, NULL, NULL))
				{
					unsigned __int64 i64FileSize = (unsigned __int64)giBufferSize * (unsigned __int64)giRounds;

					if (i64FileSize >= FreeBytesAvailable.QuadPart)
					{
						char sSize[64];
						sprintf_s(sText, "The selected volume has insufficient disk space to support the test file: (%s).",
							FileSizeFriendly((__int64)i64FileSize, sSize, sizeof(sSize)));
						MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
						return true;
					}
				}
				else {
					sprintf_s(sText, "Failed to retrieve free space information for %s.", gsDrive);
					MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}
			}
			else {
				sprintf_s(sText, "Failed to retrieve volume information for %s.", gsDrive);
				MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "BlockSize", gsLastChunkSize);
			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "BlocksPerRun", gsLastIOS);
			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "Runs", gsLastIterations);
			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "LastDrive", gsDrive);
			Set_BOOLRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "AllowCache", gbUseWriteCache);

			EndDialog(hWnd, 100);
			return TRUE;
		}

		if (wParam == IDC_CMDCANCEL || wParam == IDCANCEL) //- Cancel Button.
		{
			EndDialog(hWnd, 0);
			DestroyWindow(hWnd);

			hConfigDialog = NULL;

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

		//- Any painting should be done here.

		EndPaint(hWnd, &ThisPS);
		return TRUE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_SYSCOMMAND) //- Received a system menu message.
	{
		if (LOWORD(wParam) == CONFIGMENU_MENU_ABOUT) //- About.
		{
			_AboutDialogInfo ADI;

			ADI.DisplayIcon = LoadIcon(ghAppInstance, MAKEINTRESOURCE(IDI_MAIN));
			ADI.TitleCaption = gsTitleCaption;
			ADI.FileVersion = gsFileVersion;
			ADI.BuildDate = __DATE__;
			ADI.BuildTime = __TIME__;
			ADI.CopyRight = gsAppCopyRight;
			ADI.OwnerHandle = hWnd;

			NetLogo(&ADI);
			return TRUE;
		}

		return FALSE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_CLOSE) //- Received close message.
	{
		EndDialog(hWnd, 0);
		DestroyWindow(hWnd);

		hConfigDialog = NULL;

		return TRUE;
	}

	//--------------------------------------------------------------------------

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

