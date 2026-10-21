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
	static HWND hBlockSize = NULL;
	static HWND hFileSize = NULL;
	static HWND hDuration = NULL;

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
		hBlockSize = GetDlgItem(hWnd, IDC_BLOCKSIZE);
		hFileSize = GetDlgItem(hWnd, IDC_FILESIZE);
		hDuration = GetDlgItem(hWnd, IDC_DURATION);

		PopDiskInformation(hDiskDrive);

		SelectComboItem(hDiskDrive, gsDrive, 3);
		Set_Text(hBlockSize, gsLastBlockSize);
		Set_Text(hFileSize, gsLastFileSize);
		Set_Text(hDuration, gsLastDuration);

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
		if (wParam == IDC_CMDABOUT)
		{
			ShowAbout(hWnd);
			return TRUE;
		}

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

			double dBlockSize = 0, dFileSize = 0, dDuration = 0;

			struct { HWND hEdit; char* sText; int iTextSz; double* pdValue; const char* sName; } fields[3] = {
				{ hBlockSize, gsLastBlockSize, (int)sizeof(gsLastBlockSize), &dBlockSize, "Block size" },
				{ hFileSize, gsLastFileSize, (int)sizeof(gsLastFileSize), &dFileSize, "Test file size" },
				{ hDuration, gsLastDuration, (int)sizeof(gsLastDuration), &dDuration, "Duration" }
			};

			for (int i = 0; i < 3; i++)
			{
				if (Get_Text(fields[i].hEdit, fields[i].sText, fields[i].iTextSz) <= 0)
				{
					sprintf_s(sText, sizeof(sText), "You must enter a value for \"%s\".", fields[i].sName);
					MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				if (mathParser.Calculate(fields[i].sText, fields[i].pdValue) != CMathParser::ResultOk)
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

			if (dBlockSize < 1)
			{
				MessageBox(hWnd, "The \"Block size\" must be greater than zero.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			else if (dBlockSize > (double)(ms.ullTotalPhys / 4) || dBlockSize > 1073741824.0)
			{
				char sSize[64];
				sprintf_s(sText, "The \"Block size\" cannot exceed 1/4th of physical RAM (%s) or 1 GB.",
					FileSizeFriendly((__int64)(ms.ullTotalPhys / 4), sSize, sizeof(sSize)));
				MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			giBlockSize = (int)dBlockSize;

			if (dDuration < 1 || dDuration > 86400)
			{
				MessageBox(hWnd, "\"Duration\" must be between 1 and 86,400 seconds.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			giDurationSeconds = (int)dDuration;

			// Reads are always unbuffered, so the block size must always be a sector multiple.
			int iSectorAligned = SectorAlign(gsDrive, giBlockSize);
			if (iSectorAligned <= 0)
			{
				MessageBox(hWnd, "Failed to determine the sector size of the selected volume.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			giBlockSize = iSectorAligned;

			if (dFileSize < (double)giBlockSize)
			{
				MessageBox(hWnd, "The \"Test file size\" must be at least one block.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}
			giFileSize = (__int64)dFileSize;

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
					unsigned __int64 i64FileSize = (unsigned __int64)giFileSize;

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

			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "BlockSize", gsLastBlockSize);
			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "FileSize", gsLastFileSize);
			Set_StringRegistryValue(HKEY_CURRENT_USER, gsRegistryKey, "Duration", gsLastDuration);
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
			ShowAbout(hWnd);
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
