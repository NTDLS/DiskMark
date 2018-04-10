#ifndef _MainDlg_CPP
#define _MainDlg_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>
#include <ShlObj.H>

#include "../Resources/Resource.H"

#include "NSWFL/NSWFL.H"
#include "CGraph/CGraph.H"
#include "CDuration/CDuration.H"

#include "../Source/Entry.H"
#include "../Source/Routines.H"

#include "../Dialogs/ConfigDlg.H"
#include "../Dialogs/MainDlg.H"
#include "../Dialogs/NetLogoDlg.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace NSWFL::Windows;
using namespace NSWFL::Conversion;
using namespace NSWFL::File;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------(Variable Declarations)
HWND hMainDialog = NULL;

HWND hAllowCache = NULL;
HWND hIOS = NULL;
HWND hIterations = NULL;
HWND hChunkSize = NULL;
HWND hBytesPerIteration = NULL;
HWND hTotalDatasetSize = NULL;

HWND hWriteDiskTime = NULL;
HWND hWriteLast = NULL;
HWND hWriteAvg = NULL;
HWND hWriteMin = NULL;
HWND hWriteMax = NULL;

HWND hReadDiskTime = NULL;
HWND hReadLast = NULL;
HWND hReadAvg = NULL;
HWND hReadMin = NULL;
HWND hReadMax = NULL;

HWND hProgress = NULL;
HWND hWriteGraph = NULL;
HWND hReadGraph = NULL;

CGraph *pWriteGraph;
CGraph *pReadGraph;

char gsDrive[MAX_PATH];
int giBufferSize = 0;
int giRuns = 0;
int giRounds = 0;
bool gbUseWriteCache = false;

HANDLE hThread = NULL;
DWORD dwThreadID = 0;
bool gbCancel = false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GraphTextFormatCallback(CGraph *pGraph, GRAPHSERIES *pSeries, GRAPHTEXTATTRIBUTES *pTextAttributes)
{
	pTextAttributes->BGMode = TRANSPARENT;
	pTextAttributes->FGColor = pSeries->LineColor;
	pTextAttributes->FontSize = 10;

	char sSize[128];
	char sSpace[20];
	char *sPart = NULL;

	FileSizeFriendly(pSeries->LastValue, 2, sSize, sizeof(sSize));

	if(pSeries->Index == 0)
	{
		if(pGraph == pWriteGraph)
		{
			GRAPHTEXTATTRIBUTES gta;
			memcpy_s(&gta, sizeof(gta), pTextAttributes, sizeof(GRAPHTEXTATTRIBUTES));
			strcpy_s(gta.Text, sizeof(gta.Text) , "Writes:");
			gta.x = 5;
			gta.y = 5;
			gta.FGColor = RGB(0, 255, 255);

			pGraph->WriteText(&gta);
		}
		else if(pGraph == pReadGraph)
		{
			GRAPHTEXTATTRIBUTES gta;
			memcpy_s(&gta, sizeof(gta), pTextAttributes, sizeof(GRAPHTEXTATTRIBUTES));
			strcpy_s(gta.Text, sizeof(gta.Text) , "Reads:");
			gta.x = 5;
			gta.y = 5;
			gta.FGColor = RGB(0, 255, 255);

			pGraph->WriteText(&gta);
		}

		pTextAttributes->x = pGraph->Width();
		pTextAttributes->y = 5;
		sPart = " Min";
	}
	else if(pSeries->Index == 1)
	{
		pTextAttributes->x = pGraph->Width();
		pTextAttributes->y = 20;
		sPart = " Max";
	}
	else if(pSeries->Index == 2)
	{
		pTextAttributes->x = pGraph->Width() - 150;
		pTextAttributes->y = 20;
		sPart = " Avg";
	}
	else {
		pTextAttributes->x = pGraph->Width() - 150;
		pTextAttributes->y = 5;
		sPart = "Last";
	}

	size_t iLen = strlen(sPart) + strlen(sSize) + 2;

	sSpace[0] = '\0';
	for(size_t i = iLen; i < 16; i++)
	{
		strcat_s(sSpace, sizeof(sSpace), " ");
	}

	sprintf_s(pTextAttributes->Text, sizeof(pTextAttributes->Text), "%s:%s%s", sPart, sSpace, sSize);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ResetCounters(void)
{
	pReadGraph->Reset();
	pWriteGraph->Reset();

	Set_Text(hAllowCache, "n/a");
	Set_Text(hIOS, "n/a");
	Set_Text(hIterations, "n/a");
	Set_Text(hChunkSize, "n/a");
	Set_Text(hBytesPerIteration, "n/a");
	Set_Text(hTotalDatasetSize, "n/a");

	Set_Text(hWriteDiskTime, "n/a");
	Set_Text(hWriteLast, "n/a");
	Set_Text(hWriteAvg, "n/a");
	Set_Text(hWriteMin, "n/a");
	Set_Text(hWriteMax, "n/a");

	Set_Text(hReadDiskTime, "n/a");
	Set_Text(hReadLast, "n/a");
	Set_Text(hReadAvg, "n/a");
	Set_Text(hReadMin, "n/a");
	Set_Text(hReadMax, "n/a");

	SendMessage(hProgress, PBM_SETPOS, 0, (LPARAM) 0);

	pReadGraph->Invalidate();
	pWriteGraph->Invalidate();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PerformWriteTests(const char *sDrive, int iBufferSize, int iRuns, int iRounds, bool bUseWriteCache)
{
	ResetCounters();

	SetThreadAffinityMask(GetCurrentThread(), 0);

	CDuration timer;

    char *sBuf = NULL;
	char sFileName[MAX_PATH];
	char sTemp[1024];
	char sTemp2[1024];

	double dTotalMBPerSecond = 0;
	double dMBPerSecond = 0;
	double dDuration = 0;
	double dTotalDuration = 0;
	double dAverageMBPerSecond = 0;

	double dMaxMBPS = -999999999999999;
	double dMinMBPS = 999999999999999;

	DWORD dwWritten = 0;

	HANDLE hFile = NULL;

	int iBufSz = 0;

	srand(GetTickCount());

	//-------------------------------------------------------------------------------------------------------------

	if(bUseWriteCache)
	{
		strcpy_s(sTemp, sizeof(sTemp), "Yes");
	}
	else{
		strcpy_s(sTemp, sizeof(sTemp), "No");
	}
	Set_Text(hAllowCache, sTemp);

	FormatInteger(sTemp, sizeof(sTemp), iRounds);
	Set_Text(hIOS, sTemp);

	FormatInteger(sTemp, sizeof(sTemp), iRuns);
	Set_Text(hIterations, sTemp);

	FileSizeFriendly(iBufferSize, sTemp, sizeof(sTemp));
	Set_Text(hChunkSize, sTemp);

	FileSizeFriendly(((__int64)iBufferSize * ((__int64)iRounds)), sTemp, sizeof(sTemp));
	Set_Text(hBytesPerIteration, sTemp);

	FileSizeFriendly(((__int64)iBufferSize * ((__int64)iRounds) * ((__int64)iRuns)), sTemp, sizeof(sTemp));
	Set_Text(hTotalDatasetSize, sTemp);

	SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM) iRuns * 2);

	//-------------------------------------------------------------------------------------------------------------

	//Allocate and fill the random buffer.
	sBuf = (char *) gMem.Allocate(sizeof(char), iBufferSize + 1);
	for(int i = 0; i < iBufferSize; i++)
	{
		sBuf[i] = (char) rand();
	}

	sprintf_s(sFileName, sizeof(sFileName),
		"%s\\%d-%d-%d-%d.txt",
		sDrive, iRuns, iRounds, iBufferSize, GetTickCount());
	CorrectReversePath(sFileName, sizeof(sFileName));

	DWORD dwWriteFlags = 0;

	if(!gbUseWriteCache)
	{
		dwWriteFlags = FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;
	}

	hFile = CreateFile(
		sFileName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS, 
		dwWriteFlags,
		NULL
	);

	if(hFile == INVALID_HANDLE_VALUE)
	{
		sprintf_s(sTemp, sizeof(sTemp), "Failed to create temporary write buffering file on the specified volume.\r\n\"%s\"", sFileName);
		MessageBox(hMainDialog, sTemp, gsTitleCaption, MB_ICONERROR);
		gMem.Free(sBuf);
		return false;
	}

	for(int iRun = 0; iRun < iRuns * 2; iRun++)
	{
		if(iRun == iRuns)
		{
			dTotalMBPerSecond = 0;
			dMBPerSecond = 0;
			dDuration = 0;
			dTotalDuration = 0;
			dAverageMBPerSecond = 0;

			dMaxMBPS = -999999999999999;
			dMinMBPS = 999999999999999;

			CloseHandle(hFile);

			DWORD dwReadFlags = 0;

			if(!gbUseWriteCache)
			{
				dwReadFlags = FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
			}

			hFile = CreateFile(
				sFileName,
				GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING, 
				dwReadFlags,
				NULL
			);

			if(hFile == INVALID_HANDLE_VALUE)
			{
				sprintf_s(sTemp, sizeof(sTemp), "Failed to create temporary read buffering file on the specified volume. \r\n\"%s\"", sFileName);
				MessageBox(hMainDialog, sTemp, gsTitleCaption, MB_ICONERROR);
				gMem.Free(sBuf);
				return false;
			}
		}

		FileSeek(hFile, 0, FILE_BEGIN);

		timer.Start();
		for(int iRound = 0; iRound < iRounds; iRound++)
		{
			if(iRun < iRuns)
			{
				if(!WriteFile(hFile, sBuf, iBufferSize, &dwWritten, NULL))
				{
					MessageBox(hMainDialog, "Failed to [write] the file buffer."
						" Access was denied or the buffer is not sector aligned.", gsTitleCaption, MB_ICONERROR);
					gbCancel = true;
				}
			}
			else {
				if(!ReadFile(hFile, sBuf, iBufferSize, &dwWritten, NULL))
				{
					MessageBox(hMainDialog, "Failed to [read] the file buffer."
						" Access was denied or the buffer is not sector aligned.", gsTitleCaption, MB_ICONERROR);
					gbCancel = true;
				}
			}

			if(gbCancel)
			{
				break;
			}
		}
		timer.Stop();

		dDuration = timer.Calculate() / 1000.0; // Duration of last write operation.

		if(dDuration <= 0)
		{
			MessageBox(hMainDialog,
				"The selected disk completed the write operation to quickly for measurement."
				" Try increasing some of the stress values for a more accurate test.",
				gsTitleCaption, MB_ICONINFORMATION);

			gbCancel = true;
		}

		if(!gbCancel)
		{
			int iRunAvg = iRun;

			if(iRun >= iRuns)
			{
				iRunAvg -= iRuns;
			}

			dTotalDuration = (dTotalDuration + dDuration); //Total Disk Time.
			dMBPerSecond = (((__int64)iBufferSize * (__int64)iRounds) / dDuration); //Last MB/s.
			dTotalMBPerSecond += dMBPerSecond / 1024.0; //Sum of all MB/s calculations. Used for averages.
			dAverageMBPerSecond = (dTotalMBPerSecond / ((double)(iRunAvg + 1))) * 1024.0; //Average MB/s.

			if(dMBPerSecond > dMaxMBPS)
			{
				dMaxMBPS = dMBPerSecond;
			}

			if(dMBPerSecond < dMinMBPS)
			{
				dMinMBPS = dMBPerSecond;
			}

			double dValues[4];
			dValues[0] = dMinMBPS;
			dValues[1] = dMaxMBPS;
			dValues[2] = dAverageMBPerSecond;
			dValues[3] = dMBPerSecond;

			if(iRun < iRuns)
			{
				pWriteGraph->Update(dValues);
				pWriteGraph->Invalidate();

				FormatDouble(sTemp, sizeof(sTemp), dTotalDuration, 2);
				sprintf_s(sTemp2, sizeof(sTemp2), " %ss", sTemp);
				Set_Text(hWriteDiskTime, sTemp2);

				FileSizeFriendly((__int64)dMBPerSecond, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hWriteLast, sTemp2);

				FileSizeFriendly((__int64)dAverageMBPerSecond, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hWriteAvg, sTemp2);

				FileSizeFriendly((__int64)dMinMBPS, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hWriteMin, sTemp2);

				FileSizeFriendly((__int64)dMaxMBPS, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hWriteMax, sTemp2);
			}
			else {
				pReadGraph->Update(dValues);
				pReadGraph->Invalidate();

				FormatDouble(sTemp, sizeof(sTemp), dTotalDuration, 2);
				sprintf_s(sTemp2, sizeof(sTemp2), " %ss", sTemp);
				Set_Text(hReadDiskTime, sTemp2);

				FileSizeFriendly((__int64)dMBPerSecond, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hReadLast, sTemp2);

				FileSizeFriendly((__int64)dAverageMBPerSecond, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hReadAvg, sTemp2);

				FileSizeFriendly((__int64)dMinMBPS, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hReadMin, sTemp2);

				FileSizeFriendly((__int64)dMaxMBPS, sTemp, sizeof(sTemp));
				sprintf_s(sTemp2, sizeof(sTemp2), " %s/s", sTemp);
				Set_Text(hReadMax, sTemp2);
			}

			SendMessage(hProgress, PBM_SETPOS, iRun, (LPARAM) 0);
		}
		else {
			break;
		}
	}

	SendMessage(hProgress, PBM_SETPOS, iRuns * 2, (LPARAM) 0);

	CloseHandle(hFile);
	DeleteFile(sFileName);

	gMem.Free(sBuf);

	if(gbCancel)
	{
		MessageBox(hMainDialog, "Cancelled.", gsTitleCaption, MB_ICONINFORMATION);
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI ThreadProc(LPVOID lpVoid)
{
	char sCaption[1024];
	sprintf_s(sCaption, sizeof(sCaption), "%s (%s)", gsTitleCaption, gsDrive);
	SendMessage(hMainDialog, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)sCaption);

	Set_Text(GetDlgItem(hMainDialog, ID_CMDMULTI), "Cancel");

	if(PerformWriteTests(gsDrive, giBufferSize, giRuns, giRounds, gbUseWriteCache))
	{
		MessageBox(hMainDialog, "Complete!", gsTitleCaption, MB_ICONINFORMATION);
	}

	gbCancel = false;
	Set_Text(GetDlgItem(hMainDialog, ID_CMDMULTI), "Run");
	EnableWindow(GetDlgItem(hMainDialog, ID_CMDMULTI), TRUE);

	CloseHandle(hThread);
	hThread = NULL;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK MainDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool bFirstPaint = true;

	//--------------------------------------------------------------------------

    if(uMsg == WM_INITDIALOG)
    {
        hMainDialog = hWnd;

        HMENU SystemMenu_hMenu = GetSystemMenu(hWnd, FALSE);
        AppendMenu(SystemMenu_hMenu, MF_SEPARATOR, 0, 0);
        AppendMenu(SystemMenu_hMenu, MF_STRING, CONFIGMENU_MENU_ABOUT, "About");

		SendMessage(hWnd, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)gsTitleCaption);
		SendMessage(hWnd, WM_SETICON, TRUE, (LPARAM) LoadIcon(ghAppInstance, MAKEINTRESOURCE(IDI_MAIN)));

		hAllowCache			= GetDlgItem(hWnd, IDC_ALLOWCACHE);
		hIOS				= GetDlgItem(hWnd, IDC_IOS);
		hIterations			= GetDlgItem(hWnd, IDC_ITERATIONS);
		hChunkSize			= GetDlgItem(hWnd, IDC_CHUNKSIZE);
		hBytesPerIteration	= GetDlgItem(hWnd, IDC_BYTESPERITERATION);
		hTotalDatasetSize	= GetDlgItem(hWnd, IDC_TOTALDATASETSIZE);
		hProgress			= GetDlgItem(hWnd, IDC_DSKPROGRESS);

		hWriteGraph		= GetDlgItem(hWnd, IDC_GRAPHWRITE);
		hWriteDiskTime  = GetDlgItem(hWnd, IDC_DISKTIMEWRITE);
		hWriteLast      = GetDlgItem(hWnd, IDC_LASTWRITE);
		hWriteAvg       = GetDlgItem(hWnd, IDC_AVGWRITE);
		hWriteMin       = GetDlgItem(hWnd, IDC_MINWRITE);
		hWriteMax       = GetDlgItem(hWnd, IDC_MAXWRITE);

		hReadGraph      = GetDlgItem(hWnd, IDC_GRAPHREAD);
		hReadDiskTime   = GetDlgItem(hWnd, IDC_DISKTIMEREAD);
		hReadLast       = GetDlgItem(hWnd, IDC_LASTREAD);
		hReadAvg        = GetDlgItem(hWnd, IDC_AVGREAD);
		hReadMin        = GetDlgItem(hWnd, IDC_MINREAD);
		hReadMax        = GetDlgItem(hWnd, IDC_MAXREAD);

		pWriteGraph = new CGraph(hWriteGraph, RGB(0, 0, 0), RGB(0, 100, 0), 15, GRAPH_LEFT, true);
		pWriteGraph->SpacingTop(50);

		pWriteGraph->AddSeries(RGB(255, 0, 0), 1024.0, GraphTextFormatCallback);
		pWriteGraph->AddSeries(RGB(0, 255, 0), 1024.0, GraphTextFormatCallback);
		pWriteGraph->AddSeries(RGB(200, 200, 200), 1024.0, GraphTextFormatCallback);
		pWriteGraph->AddSeries(RGB(255, 255, 0), 1024.0, GraphTextFormatCallback);

		pReadGraph = new CGraph(hReadGraph, RGB(0, 0, 0), RGB(0, 100, 0), 15, GRAPH_LEFT, true);
		pReadGraph->SpacingTop(50);

		pReadGraph->AddSeries(RGB(255, 0, 0), 1024.0, GraphTextFormatCallback);
		pReadGraph->AddSeries(RGB(0, 255, 0), 1024.0, GraphTextFormatCallback);
		pReadGraph->AddSeries(RGB(200, 200, 200), 1024.0, GraphTextFormatCallback);
		pReadGraph->AddSeries(RGB(255, 255, 0), 1024.0, GraphTextFormatCallback);

		ResetCounters();

		CenterWindow(hWnd);

        return TRUE; // Return TRUE to set the keyboard focus, Otherwise return FALSE.
    }

	//--------------------------------------------------------------------------

    if(uMsg == WM_COMMAND)
    {
        if(wParam == ID_CMDMULTI) //- Cancel Button.
        {
			char sCaption[100];
			Get_Text(GetDlgItem(hWnd, ID_CMDMULTI), sCaption, sizeof(sCaption));
			if(_strcmpi(sCaption, "Run") == 0)
			{
				if(DialogBox(ghAppInstance, MAKEINTRESOURCE(IDD_CONFIG), hWnd, ConfigDialog) == 100)
				{
					hThread = CreateThread(NULL, 0, ThreadProc, (LPVOID)0, 0, &dwThreadID);
				}
				return true;
			}
			else {
				if(MessageBox(hMainDialog, "Are you sure you want to cancel the operation?", gsTitleCaption, MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION) == IDYES)
				{
					if(hThread)
					{
						gbCancel = true;
						EnableWindow(GetDlgItem(hMainDialog, ID_CMDMULTI), FALSE);
					}
					return TRUE;
				}
			}
        }
        else if(wParam == IDC_CMDHELP) //- Ok Button.
        {
			OpenHelp(hWnd);
			return TRUE;
		}
		else if(wParam == IDC_ONTOP)
        {
            if(IsDlgButtonChecked(hWnd, IDC_ONTOP))
			{
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			else {
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
		}

        return FALSE;
    }

	//--------------------------------------------------------------------------

	if(uMsg == WM_PAINT)
    {
		if(bFirstPaint)
		{
			bFirstPaint = false;
			if(DialogBox(ghAppInstance, MAKEINTRESOURCE(IDD_CONFIG), hWnd, ConfigDialog) == 100)
			{
				hThread = CreateThread(NULL, 0, ThreadProc, (LPVOID)0, 0, &dwThreadID);
			}
		}

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
		if(hThread)
		{
			MessageBox(hMainDialog, "Cancel the operation before closing.", gsTitleCaption, MB_ICONINFORMATION);
		}
		else {
			delete pWriteGraph;
			delete pReadGraph;

			EndDialog(hWnd,0);
			DestroyWindow(hWnd);

			hMainDialog = NULL;
			return true;
		}


        return TRUE;
    }

    //--------------------------------------------------------------------------

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

