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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::File;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpenHelp(HWND hWnd)
{
	char sFileName[MAX_PATH];
	char sPath[MAX_PATH];
	char sDrive[MAX_PATH];
	char sDir[MAX_PATH];

	if(GetModuleFileName(ghAppInstance, sFileName, sizeof(sFileName)))
	{
		_splitpath_s(sFileName, sDrive, sizeof(sDrive), sDir, sizeof(sDir), NULL, 0, NULL, 0);
		_makepath_s(sFileName, sizeof(sFileName), sDrive, sDir, "help", ".chm");
		_makepath_s(sPath, sizeof(sPath), sDrive, sDir, NULL, NULL);

		if(ShellExecute(hWnd, "OPEN", sFileName, NULL, sPath, SW_SHOWDEFAULT) <= (HINSTANCE)32)
		{
			MessageBox(hWnd, "Cannot locate the Help File.", gsTitleCaption, MB_ICONERROR);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int SectorAlign(const char *sDrive, int iBufferSize)
{
	char sFileName[MAX_PATH];

	sprintf_s(sFileName, sizeof(sFileName),
		"%s\\%d-%d.txt",
		sDrive, iBufferSize, GetTickCount());
	CorrectReversePath(sFileName, sizeof(sFileName));

	HANDLE hFile = CreateFile(
		sFileName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS, 
		FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
		NULL
	);

	if((iBufferSize % 512) != 1)
	{
		if(iBufferSize > 512)
		{
			iBufferSize -= iBufferSize % 512;
		}
		else {
			iBufferSize = 512;
		}
	}

	DWORD dwBytesWritten = 0;
	DWORD dwBytesToWrite = iBufferSize;

	char *sBuffer = (char *) calloc(iBufferSize + 1025, 1);

	for(DWORD iTries = 0; iTries < 1024; iTries++)
	{
		if(WriteFile(hFile, sBuffer, dwBytesToWrite, &dwBytesWritten, NULL))
		{
			if(dwBytesWritten == dwBytesToWrite)
			{
				CloseHandle(hFile);
				DeleteFile(sFileName);
				free(sBuffer);
				return (int)dwBytesToWrite;
			}
		}

		dwBytesToWrite++;
	}

	CloseHandle(hFile);
	DeleteFile(sFileName);
	free(sBuffer);
	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
