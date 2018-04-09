#ifndef _ENTRY_CPP
#define _ENTRY_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "../Resources/Resource.H"

#include "../../@Libraries//NSWFL/NSWFL.H"

#include "Entry.H"
#include "Init.H"
#include "Routines.H"

#include "../Dialogs/ConfigDlg.H"
#include "../Dialogs/MainDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HINSTANCE ghAppInstance = NULL;

char gsAppCopyRight[64];
char gsFileVersion[32];
char gsTitleCaption[64];
char gsRegistryKey[255];

char gsLastChunkSize[1024];
char gsLastIOS[1024];
char gsLastIterations[1024];

CMemPool gMem; //The global memory object.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
int main(int argc, char *argv[])
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR sCmdLine, int iCmdShow)
#endif
{
    #ifdef _DEBUG
	ghAppInstance = GetModuleHandle(NULL);
    #else
	ghAppInstance = hInstance;
    #endif

	if(!InitializeApp())
    {
        return 1;
    }

	DialogBox(ghAppInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDialog);

    if(!UninitializeApp())
    {
        return 1;
    }

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

