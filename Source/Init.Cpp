#ifndef _INIT_CPP
#define _INIT_CPP
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

#include "../Dialogs/MainDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::Registry;
using namespace NSWFL::Memory;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InitializeApp(void)
{
	if(!gMem.Initialize())
	{
		return false;
	}
	
	LoadString(ghAppInstance, IDS_APPCOPYRIGHT, gsAppCopyRight, sizeof(gsAppCopyRight));
    LoadString(ghAppInstance, IDS_FILEVERSION, gsFileVersion, sizeof(gsFileVersion));
    LoadString(ghAppInstance, IDS_TITLECAPTION, gsTitleCaption, sizeof(gsTitleCaption));
    LoadString(ghAppInstance, IDS_REGISTRYKEY, gsRegistryKey, sizeof(gsRegistryKey));

	DWORD dwsRegValueSz = 0;

	dwsRegValueSz = sizeof(gsDrive);
	if(!Get_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "LastDrive", gsDrive, dwsRegValueSz))
	{
		strcpy_s(gsDrive, sizeof(gsDrive), "");
	}

	dwsRegValueSz = sizeof(gsLastChunkSize);
	if(!Get_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "ChunkSize", gsLastChunkSize, dwsRegValueSz))
	{
		strcpy_s(gsLastChunkSize, sizeof(gsLastChunkSize), "1024 * 64");
	}

	dwsRegValueSz = sizeof(gsLastIOS);
	if(!Get_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "IOS", gsLastIOS, dwsRegValueSz))
	{
		strcpy_s(gsLastIOS, sizeof(gsLastIOS), "128");
	}

	dwsRegValueSz = sizeof(gsLastIterations);
	if(!Get_StringRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "Iterations", gsLastIterations, dwsRegValueSz))
	{
		strcpy_s(gsLastIterations, sizeof(gsLastIterations), "320");
	}

	gbUseWriteCache = Get_BOOLRegistryValue(HKEY_LOCAL_MACHINE, gsRegistryKey, "AllowCache");

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UninitializeApp(void)
{
	gMem.Destroy();
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

