#ifndef _ConfigDlg_CPP
#define _ConfigDlg_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>

#include "../Resources/Resource.H"

#include "NSWFL/NSWFL.H"
#include "CMathParser/CMathParser.H"
#include "CStringBuilder/CStringBuilder.H"

#include "../Source/Entry.H"
#include "../Source/Routines.H"

#include "../Dialogs/ConfigDlg.H"
#include "../Dialogs/MainDlg.H"
#include "../Dialogs/NetLogoDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::Windows;
using namespace NSWFL::File;
using namespace NSWFL::Registry;

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

	while(iPos <= 90)
    {
        sprintf_s(sDrive, sizeof(sDrive), "%c:\\", iPos);

		if((iDriveType = GetDriveType(sDrive)) != DRIVE_NO_ROOT_DIR)
		{
			sprintf_s(sText, sizeof(sText), "%s", sDrive);
			
			if(iDriveType == DRIVE_UNKNOWN) {
				strcat_s(sText, sizeof(sText), " (Unknown Media)");
			}
			else if(iDriveType == DRIVE_REMOVABLE) {
				strcat_s(sText, sizeof(sText), " (Removable Media)");
			}
			else if(iDriveType == DRIVE_FIXED) {
				strcat_s(sText, sizeof(sText), " (Fixed Disk)");
			}
			else if(iDriveType == DRIVE_REMOTE) {
				strcat_s(sText, sizeof(sText), " (Network Drive)");
			}
			else if(iDriveType == DRIVE_CDROM) {
				strcat_s(sText, sizeof(sText), " (CD-ROM Drive)");
			}
			else if(iDriveType == DRIVE_RAMDISK) {
				strcat_s(sText, sizeof(sText), " (RAM disk)");
			}

			if(iDriveType != DRIVE_UNKNOWN && iDriveType != DRIVE_CDROM)
			{
				SendMessage(hDiskDrive, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)sText);
			}
		}

		iPos++;
    }

	LRESULT iIndex = 0;

	if((iIndex = SendMessage(hDiskDrive, (UINT)CB_FINDSTRING, (WPARAM)0, (LPARAM)"C:\\")) == CB_ERR)
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

    if(uMsg == WM_INITDIALOG) // Received A Initilize Dialog Message
    {
        hConfigDialog = hWnd;

        HMENU SystemMenu_hMenu = GetSystemMenu(hWnd, FALSE);
        AppendMenu(SystemMenu_hMenu, MF_SEPARATOR, 0, 0);
        AppendMenu(SystemMenu_hMenu, MF_STRING, CONFIGMENU_MENU_ABOUT, "About");

        SendMessage(hWnd, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)gsTitleCaption);
        SendMessage(hWnd, WM_SETICON, TRUE, (LPARAM) LoadIcon(ghAppInstance, MAKEINTRESOURCE(IDI_MAIN)));

        hDiskDrive	= GetDlgItem(hWnd, IDC_DISKDRIVE);
        hChunkSize	= GetDlgItem(hWnd, IDC_CHUNKSIZE);
		hIOS		= GetDlgItem(hWnd, IDC_IOS);
		hIterations = GetDlgItem(hWnd, IDC_ITERATIONS);

		SetWindowPos(
			hDiskDrive,				// Handle of window.
			NULL,					// Placement-order handle.
			0,						// Horizontal position.
			0,						// Vertical position.
			158,					// Width.
			100,					// Height.
			SWP_NOMOVE|SWP_NOZORDER // Window-positioning flags.
		);

		PopDiskInformation(hDiskDrive);

		SelectComboItem(hDiskDrive, gsDrive, 3);
		Set_Text(hChunkSize, gsLastChunkSize);
		Set_Text(hIOS, gsLastIOS);
		Set_Text(hIterations, gsLastIterations);

		if(gbUseWriteCache)
		{
			CheckDlgButton(hWnd, IDC_ALLOWCACHING, BST_CHECKED);
		}

		CenterWindow(hWnd);

        return TRUE; // Return TRUE to set the keyboard focus, Otherwise return FALSE.
    }

    //--------------------------------------------------------------------------

	if(uMsg == WM_COMMAND)
    {
        if(wParam == IDC_CMDHELP) //- Ok Button.
        {
			OpenHelp(hWnd);
			return TRUE;
		}

		if(wParam == IDC_CMDOK || wParam == IDOK) //- Ok Button.
        {
			char sText[1024];

			if(!Get_Text(hDiskDrive, gsDrive, sizeof(gsDrive)))
			{
				MessageBox(hWnd, "You must select a drive to test.", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			if(SendMessage(hDiskDrive, (UINT)CB_FINDSTRING, (WPARAM)0, (LPARAM)gsDrive) != CB_ERR)
			{
				gsDrive[3] = '\0';
			}

			if(gsDrive[1] != ':')
			{
				gsDrive[1] = ':';
			}
			if(gsDrive[2] != '\\')
			{
				gsDrive[2] = '\\';
			}

			CMathParser mathParser;
			CStringBuilder errorText;

			gbUseWriteCache = (IsDlgButtonChecked(hWnd, IDC_ALLOWCACHING) == BST_CHECKED);

			int iRegValueSz = 0;
			iRegValueSz = Get_Text(hChunkSize, gsLastChunkSize, sizeof(gsLastChunkSize));
			if(iRegValueSz > 0 || iRegValueSz < sizeof(gsLastChunkSize))
			{
				MEMORYSTATUSEX ms;
				memset(&ms, 0, sizeof(ms));
				ms.dwLength = sizeof(ms);
				GlobalMemoryStatusEx(&ms);

				if(!mathParser.Calculate(gsLastChunkSize, &giBufferSize) == CMathParser::ResultOk)
				{
					errorText.Append("An error occured parsing the \"Buffer Size\": ");
					errorText.Append(mathParser.LastError()->Text);
					MessageBox(hWnd, errorText.Buffer, gsTitleCaption, MB_ICONINFORMATION);
					return false;
				}

				if(giBufferSize < 1)
				{
					MessageBox(hWnd, "The \"Buffer Size\" must be greater than zero.", gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}
				else if(giBufferSize > (ms.ullTotalPhys / 4))
				{
					char sSize[64];
					sprintf_s(sText, "The \"Buffer Size\" cannot exceed 1/4th of physical RAM (%s)",
						FileSizeFriendly((ms.ullTotalPhys / 4), sSize, sizeof(sSize)));
					MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				if(!gbUseWriteCache)
				{
					int iSectorAligned = SectorAlign(gsDrive, giBufferSize);
					if(iSectorAligned <= 0)
					{
						MessageBox(hWnd,
							"Failed to sector align the disk buffer. See your hardware documentation for proper sector sizes or enable disk caching.",
							gsTitleCaption, MB_ICONINFORMATION);
						return true;
					}

					giBufferSize = iSectorAligned;
				}

				Set_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "BufferSize", gsLastChunkSize);
			}
			else {
				MessageBox(hWnd, "You must enter a \"Buffer Size\".", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			iRegValueSz = Get_Text(hIterations, gsLastIterations, sizeof(gsLastIterations));
			if(iRegValueSz > 0 || iRegValueSz < sizeof(gsLastIterations))
			{
				if(!mathParser.Calculate(gsLastIterations, &giRuns) == CMathParser::ResultOk)
				{
					errorText.Append("An error occured parsing the \"Buffer Size\": ");
					errorText.Append(mathParser.LastError()->Text);
					MessageBox(hWnd, errorText.Buffer, gsTitleCaption, MB_ICONINFORMATION);
					return false;
				}

				if(giRuns < 1 || giRuns > 10000)
				{
					MessageBox(hWnd, "\"Runs\" must be between 1 and 10,000.", gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				Set_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "Runs", gsLastIterations);
			}
			else {
				MessageBox(hWnd, "You must enter a number of \"Runs\".", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			iRegValueSz = Get_Text(hIOS, gsLastIOS, sizeof(gsLastIOS));
			if(iRegValueSz > 0 || iRegValueSz < sizeof(gsLastIOS))
			{
				if(!mathParser.Calculate(gsLastIOS, &giRounds) == CMathParser::ResultOk)
				{
					errorText.Append("An error occured parsing the \"Buffer Size\": ");
					errorText.Append(mathParser.LastError()->Text);
					MessageBox(hWnd, errorText.Buffer, gsTitleCaption, MB_ICONINFORMATION);
					return false;
				}

				if(giRounds < 1 || giRounds > 10000)
				{
					MessageBox(hWnd, "The \"Rounds\" must be between 1 and 10,000.", gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				Set_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "Rounds", gsLastIOS);
			}
			else {
				MessageBox(hWnd, "You must enter a number of \"Rounds\".", gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			char sVolumeNameBuffer[1024];
			DWORD dwFileSystemFlags;
			char sFileSystemNameBuffer[1024];

			if(GetVolumeInformation(gsDrive,
				sVolumeNameBuffer,
				sizeof(sVolumeNameBuffer),
				NULL,
				NULL,
				&dwFileSystemFlags,
				sFileSystemNameBuffer,
				sizeof(sFileSystemNameBuffer)))
			{

				if(dwFileSystemFlags &FILE_READ_ONLY_VOLUME)
				{
					MessageBox(hWnd, "The selected volume is read-only.", gsTitleCaption, MB_ICONINFORMATION);
					return true;
				}

				ULARGE_INTEGER FreeBytesAvailable;
				if(GetDiskFreeSpaceEx(gsDrive, &FreeBytesAvailable, NULL, NULL))
				{
					unsigned __int64 i64BufferSize = giBufferSize;
					unsigned __int64 i64Rounds = giRounds;

					if((i64BufferSize * i64Rounds) >= FreeBytesAvailable.QuadPart)
					{
						char sSize[64];
						sprintf_s(sText, "The selected volume has insufficient disk space to support the test file: (%s).",
							FileSizeFriendly(giBufferSize * giRounds, sSize, sizeof(sSize)));
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
				sprintf_s(sText, "Failed to retrieve volumne information for %s.", gsDrive);
				MessageBox(hWnd, sText, gsTitleCaption, MB_ICONINFORMATION);
				return true;
			}

			Set_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "LastDrive", gsDrive);
			Set_BOOLRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "AllowCache", gbUseWriteCache);

			EndDialog(hWnd, 100);
			return TRUE;
        }

        if(wParam == IDC_CMDCANCEL || wParam == IDCANCEL) //- Cancel Button.
        {
            EndDialog(hWnd, 0);
            DestroyWindow(hWnd);

			hConfigDialog = NULL;

            return TRUE;
        }

        return FALSE;
    }

    //--------------------------------------------------------------------------

	if(uMsg == WM_PAINT)
    {
        HDC ThisHDC;
        PAINTSTRUCT ThisPS;

        ThisHDC = BeginPaint(hWnd, &ThisPS);

        //- Any painting should be done here.

        EndPaint(hWnd, &ThisPS);
        return TRUE;
    }

    //--------------------------------------------------------------------------

    if(uMsg == WM_SYSCOMMAND) //- Received a system menu message.
    {
        if(LOWORD(wParam) == CONFIGMENU_MENU_ABOUT) //- About.
        {
            _AboutDialogInfo ADI;
    
            ADI.DisplayIcon  = LoadIcon(ghAppInstance, MAKEINTRESOURCE(IDI_MAIN));
            ADI.TitleCaption = gsTitleCaption;
            ADI.FileVersion  = gsFileVersion;
            ADI.BuildDate    = __DATE__;
            ADI.BuildTime    = __TIME__;
            ADI.CopyRight    = gsAppCopyRight;
            ADI.OwnerHandle  = hWnd;

            NetLogo(&ADI);
            return TRUE;
        }

        return FALSE;
    }

    //--------------------------------------------------------------------------

    if(uMsg == WM_CLOSE) //- Received close message.
    {
        EndDialog(hWnd,0);
        DestroyWindow(hWnd);

		hConfigDialog = NULL;

        return TRUE;
    }

    //--------------------------------------------------------------------------

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

