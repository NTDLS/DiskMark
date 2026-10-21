#ifndef _ROUTINES_CPP
#define _ROUTINES_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <WindowsX.H>
#include <ShellAPI.H>
#include <Stdio.H>
#include <Stdlib.H>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "../Resources/Resource.H"
#include "../NSWFL/NSWFL.H"

#include "Init.H"
#include "Entry.H"
#include "Routines.H"
#include "DiskBench.H"
#include "../Dialogs/NetLogoDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::File;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpenHelp(HWND hWnd)
{
	char sFileName[MAX_PATH];
	char sPath[MAX_PATH];
	char sDrive[MAX_PATH];
	char sDir[MAX_PATH];

	if (GetModuleFileName(ghAppInstance, sFileName, sizeof(sFileName)))
	{
		_splitpath_s(sFileName, sDrive, sizeof(sDrive), sDir, sizeof(sDir), NULL, 0, NULL, 0);
		_makepath_s(sFileName, sizeof(sFileName), sDrive, sDir, "help", ".chm");
		_makepath_s(sPath, sizeof(sPath), sDrive, sDir, NULL, NULL);

		if (ShellExecute(hWnd, "OPEN", sFileName, NULL, sPath, SW_SHOWDEFAULT) <= (HINSTANCE)32)
		{
			MessageBox(hWnd, "Cannot locate the Help File.", gsTitleCaption, MB_ICONERROR);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ShowAbout(HWND hWnd)
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
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int SectorAlign(const char* sDrive, int iBufferSize)
{
	if (iBufferSize < 1)
	{
		return -1;
	}

	return (int)BenchAlignBlockSize(sDrive, (DWORD)iBufferSize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
