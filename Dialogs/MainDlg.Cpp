#ifndef _MainDlg_CPP
#define _MainDlg_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>
#include <ShlObj.H>

#include "../Resources/Resource.H"

#include "../NSWFL/NSWFL.H"
#include "../CGraph/CGraph.H"

#include "../Source/Entry.H"
#include "../Source/Routines.H"
#include "../Source/DiskBench.H"

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

HWND hDriveInfo = NULL;
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
HWND hWriteIops = NULL;
HWND hWriteLatency = NULL;

HWND hReadDiskTime = NULL;
HWND hReadLast = NULL;
HWND hReadAvg = NULL;
HWND hReadMin = NULL;
HWND hReadMax = NULL;
HWND hReadIops = NULL;
HWND hReadLatency = NULL;

HWND hProgress = NULL;
HWND hStatus = NULL;
HWND hWriteGraph = NULL;
HWND hReadGraph = NULL;

CGraph* pWriteGraph;
CGraph* pReadGraph;

HFONT ghBigFont = NULL;

char gsDrive[MAX_PATH];
int giBufferSize = 0;
int giRuns = 0;
int giRounds = 0;
bool gbUseWriteCache = false;

HANDLE hThread = NULL;
DWORD dwThreadID = 0;
volatile bool gbCancel = false;

//Results of the most recent test, for "Copy Results".
BENCHCONFIG gBenchConfig;
BENCHSTATS gWriteStats;
BENCHSTATS gReadStats;
bool gbHaveWriteStats = false;
bool gbHaveReadStats = false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define WM_APP_SHOWCONFIG (WM_APP + 1)

#define GRAPH_COLOR_MIN   RGB(239, 83, 80)
#define GRAPH_COLOR_MAX   RGB(102, 187, 106)
#define GRAPH_COLOR_AVG   RGB(176, 190, 197)
#define GRAPH_COLOR_LAST  RGB(255, 213, 79)

bool GraphTextFormatCallback(CGraph* pGraph, GRAPHSERIES* pSeries, GRAPHTEXTATTRIBUTES* pTextAttributes)
{
	pTextAttributes->BGMode = TRANSPARENT;
	pTextAttributes->FGColor = pSeries->LineColor;
	pTextAttributes->FontSize = 9;
	strcpy_s(pTextAttributes->FontName, sizeof(pTextAttributes->FontName), "Segoe UI");
	pTextAttributes->y = 5;

	//The x position is managed by the graph: each series is laid out right to left, so Min ends up right-most.

	if (pSeries->Index == 0)
	{
		//Start the legend a few pixels in from the right edge.
		pTextAttributes->x = pGraph->Width() - 8;

		//Title, top left.
		GRAPHTEXTATTRIBUTES gta;
		memcpy_s(&gta, sizeof(gta), pTextAttributes, sizeof(GRAPHTEXTATTRIBUTES));
		strcpy_s(gta.Text, sizeof(gta.Text), (pGraph == pWriteGraph) ? "Write (MB/s)" : "Read (MB/s)");
		gta.x = 6;
		gta.y = 5;
		gta.FGColor = RGB(79, 195, 247);
		pGraph->WriteText(&gta);

		//Vertical scale, bottom left.
		strcpy_s(gta.FontName, sizeof(gta.FontName), "Segoe UI");
		gta.FontSize = 8;
		gta.FGColor = RGB(120, 130, 140);
		sprintf_s(gta.Text, sizeof(gta.Text), "axis 0 - %.0f MB/s", pSeries->TopValue);
		gta.x = 6;
		gta.y = pGraph->Height() - MulDiv(16, GetDeviceCaps(gta.DC, LOGPIXELSY), 96);
		pGraph->WriteText(&gta);
	}

	static const char* sNames[] = { "Min", "Max", "Avg", "Last" };
	const char* sName = (pSeries->Index >= 0 && pSeries->Index < 4) ? sNames[pSeries->Index] : "";

	if (pSeries->LastValue > 0)
	{
		sprintf_s(pTextAttributes->Text, sizeof(pTextAttributes->Text), "%s %.1f", sName, pSeries->LastValue);
	}
	else {
		sprintf_s(pTextAttributes->Text, sizeof(pTextAttributes->Text), "%s --", sName);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void FormatRate(double dMBps, char* sOut, size_t iOutSz)
{
	sprintf_s(sOut, iOutSz, "%.1f MB/s", dMBps);
}

static void FormatLatency(double dMs, char* sOut, size_t iOutSz)
{
	if (dMs < 1.0)
	{
		sprintf_s(sOut, iOutSz, "%.0f us", dMs * 1000.0);
	}
	else {
		sprintf_s(sOut, iOutSz, "%.2f ms", dMs);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ResetCounters(void)
{
	pReadGraph->Reset();
	pWriteGraph->Reset();

	HWND hControls[] = {
		hWriteDiskTime, hWriteLast, hWriteAvg, hWriteMin, hWriteMax, hWriteIops, hWriteLatency,
		hReadDiskTime, hReadLast, hReadAvg, hReadMin, hReadMax, hReadIops, hReadLatency
	};

	for (int i = 0; i < sizeof(hControls) / sizeof(hControls[0]); i++)
	{
		Set_Text(hControls[i], "n/a");
	}

	SendMessage(hProgress, PBM_SETPOS, 0, (LPARAM)0);

	pReadGraph->Invalidate();
	pWriteGraph->Invalidate();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ShowConfiguration(const BENCHCONFIG* pConfig)
{
	char sTemp[1024];

	Set_Text(hDriveInfo, pConfig->Drive);
	Set_Text(hAllowCache, pConfig->AllowWriteCache ? "Allowed (flush timed)" : "Off (write-through)");

	FormatInteger(sTemp, sizeof(sTemp), pConfig->Runs);
	Set_Text(hIterations, sTemp);

	FormatInteger(sTemp, sizeof(sTemp), pConfig->BlocksPerRun);
	Set_Text(hIOS, sTemp);

	FileSizeFriendly((__int64)pConfig->BlockSize, sTemp, sizeof(sTemp));
	Set_Text(hChunkSize, sTemp);

	FileSizeFriendly((__int64)pConfig->BlockSize * (__int64)pConfig->BlocksPerRun, sTemp, sizeof(sTemp));
	Set_Text(hBytesPerIteration, sTemp);

	//Each phase moves the data set once.
	FileSizeFriendly((__int64)pConfig->BlockSize * (__int64)pConfig->BlocksPerRun * (__int64)pConfig->Runs, sTemp, sizeof(sTemp));
	Set_Text(hTotalDatasetSize, sTemp);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ShowStats(const BENCHSTATS* pStats)
{
	bool bWrite = (pStats->Phase == BenchPhaseWrite);
	char sTemp[128];

	FormatRate(pStats->AvgMBps, sTemp, sizeof(sTemp));
	Set_Text(bWrite ? hWriteAvg : hReadAvg, sTemp);

	FormatRate(pStats->LastMBps, sTemp, sizeof(sTemp));
	Set_Text(bWrite ? hWriteLast : hReadLast, sTemp);

	FormatRate(pStats->MinMBps, sTemp, sizeof(sTemp));
	Set_Text(bWrite ? hWriteMin : hReadMin, sTemp);

	FormatRate(pStats->MaxMBps, sTemp, sizeof(sTemp));
	Set_Text(bWrite ? hWriteMax : hReadMax, sTemp);

	sprintf_s(sTemp, sizeof(sTemp), "%.0f", pStats->Iops);
	Set_Text(bWrite ? hWriteIops : hReadIops, sTemp);

	FormatLatency(pStats->AvgLatencyMs, sTemp, sizeof(sTemp));
	Set_Text(bWrite ? hWriteLatency : hReadLatency, sTemp);

	sprintf_s(sTemp, sizeof(sTemp), "%.2f s", pStats->TotalSeconds);
	Set_Text(bWrite ? hWriteDiskTime : hReadDiskTime, sTemp);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BenchProgress(void* pContext, BenchEvent eEvent, const BENCHSTATS* pStats, int iStep, int iTotalSteps)
{
	bool bWrite = (pStats->Phase == BenchPhaseWrite);
	char sTemp[256];

	if (eEvent == BenchEventPhaseStart)
	{
		sprintf_s(sTemp, sizeof(sTemp), "%s test: starting...", bWrite ? "Write" : "Read");
		Set_Text(hStatus, sTemp);
		return;
	}

	if (bWrite)
	{
		gWriteStats = *pStats;
		gbHaveWriteStats = true;
	}
	else {
		gReadStats = *pStats;
		gbHaveReadStats = true;
	}

	ShowStats(pStats);

	double dValues[4];
	dValues[0] = pStats->MinMBps;
	dValues[1] = pStats->MaxMBps;
	dValues[2] = pStats->AvgMBps;
	dValues[3] = pStats->LastMBps;

	CGraph* pGraph = bWrite ? pWriteGraph : pReadGraph;
	//The graph advances one pixel per update. Repeat each run so short tests still span the plot as a step chart.
	int iPixelsPerRun = pGraph->Width() / (gBenchConfig.Runs > 0 ? gBenchConfig.Runs : 1);
	if (iPixelsPerRun < 1)
	{
		iPixelsPerRun = 1;
	}
	for (int i = 0; i < iPixelsPerRun; i++)
	{
		pGraph->Update(dValues);
	}
	pGraph->Invalidate();

	sprintf_s(sTemp, sizeof(sTemp), "%s test: run %d of %d", bWrite ? "Write" : "Read", pStats->RunsCompleted, gBenchConfig.Runs);
	Set_Text(hStatus, sTemp);

	SendMessage(hProgress, PBM_SETPOS, iStep, (LPARAM)0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI ThreadProc(LPVOID lpVoid)
{
	char sCaption[1024];
	sprintf_s(sCaption, sizeof(sCaption), "%s (%s)", gsTitleCaption, gsDrive);
	SendMessage(hMainDialog, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)sCaption);

	Set_Text(GetDlgItem(hMainDialog, ID_CMDMULTI), "Cancel");
	EnableWindow(GetDlgItem(hMainDialog, IDC_CMDCOPY), FALSE);

	memset(&gBenchConfig, 0, sizeof(gBenchConfig));
	strcpy_s(gBenchConfig.Drive, sizeof(gBenchConfig.Drive), gsDrive);
	gBenchConfig.BlockSize = (DWORD)giBufferSize;
	gBenchConfig.BlocksPerRun = giRounds;
	gBenchConfig.Runs = giRuns;
	gBenchConfig.AllowWriteCache = gbUseWriteCache;

	gbHaveWriteStats = false;
	gbHaveReadStats = false;

	ResetCounters();
	ShowConfiguration(&gBenchConfig);

	SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM)giRuns * 2);

	char sError[1024];
	BenchResult eResult = RunDiskBench(&gBenchConfig, &gbCancel, BenchProgress, NULL, sError, sizeof(sError));

	if (eResult == BenchOk)
	{
		char sTemp[256];
		sprintf_s(sTemp, sizeof(sTemp), "Complete. Write %.1f MB/s, Read %.1f MB/s (averages).",
			gWriteStats.AvgMBps, gReadStats.AvgMBps);
		Set_Text(hStatus, sTemp);
		SendMessage(hProgress, PBM_SETPOS, giRuns * 2, (LPARAM)0);
	}
	else if (eResult == BenchCancelled)
	{
		Set_Text(hStatus, "Cancelled.");
	}
	else {
		Set_Text(hStatus, "Failed.");
		MessageBox(hMainDialog, sError, gsTitleCaption, MB_ICONERROR);
	}

	SendMessage(hMainDialog, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)gsTitleCaption);

	gbCancel = false;
	Set_Text(GetDlgItem(hMainDialog, ID_CMDMULTI), "Run");
	EnableWindow(GetDlgItem(hMainDialog, ID_CMDMULTI), TRUE);
	EnableWindow(GetDlgItem(hMainDialog, IDC_CMDCOPY), gbHaveWriteStats || gbHaveReadStats);

	CloseHandle(hThread);
	hThread = NULL;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CopyResultsToClipboard(HWND hWnd)
{
	char sReport[4096];
	char sBlock[64];
	char sRate[64];
	char sLatency[64];

	FileSizeFriendly((__int64)gBenchConfig.BlockSize, sBlock, sizeof(sBlock));

	int iLen = sprintf_s(sReport, sizeof(sReport),
		"%s %s\r\n"
		"Drive: %s   Block size: %s   Blocks per run: %d   Runs: %d   Write cache: %s\r\n",
		gsTitleCaption, gsFileVersion, gBenchConfig.Drive, sBlock, gBenchConfig.BlocksPerRun, gBenchConfig.Runs,
		gBenchConfig.AllowWriteCache ? "allowed" : "off");

	const BENCHSTATS* pStats[2] = { &gWriteStats, &gReadStats };
	const bool bHave[2] = { gbHaveWriteStats, gbHaveReadStats };
	const char* sPhase[2] = { "Write", "Read " };

	for (int i = 0; i < 2; i++)
	{
		if (!bHave[i])
		{
			continue;
		}

		FormatLatency(pStats[i]->AvgLatencyMs, sLatency, sizeof(sLatency));
		FormatRate(pStats[i]->AvgMBps, sRate, sizeof(sRate));

		iLen += sprintf_s(sReport + iLen, sizeof(sReport) - iLen,
			"%s: avg %s, last %.1f, min %.1f, max %.1f MB/s, %.0f IOPS, %s latency, %d runs\r\n",
			sPhase[i], sRate, pStats[i]->LastMBps, pStats[i]->MinMBps, pStats[i]->MaxMBps,
			pStats[i]->Iops, sLatency, pStats[i]->RunsCompleted);
	}

	if (OpenClipboard(hWnd))
	{
		EmptyClipboard();

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, strlen(sReport) + 1);
		if (hMem)
		{
			memcpy(GlobalLock(hMem), sReport, strlen(sReport) + 1);
			GlobalUnlock(hMem);
			SetClipboardData(CF_TEXT, hMem);
		}

		CloseClipboard();
		Set_Text(hStatus, "Results copied to the clipboard.");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void PromptAndStart(HWND hWnd)
{
	if (DialogBox(ghAppInstance, MAKEINTRESOURCE(IDD_CONFIG), hWnd, ConfigDialog) == 100)
	{
		hThread = CreateThread(NULL, 0, ThreadProc, (LPVOID)0, 0, &dwThreadID);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK MainDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//--------------------------------------------------------------------------

	if (uMsg == WM_INITDIALOG)
	{
		hMainDialog = hWnd;

		HMENU SystemMenu_hMenu = GetSystemMenu(hWnd, FALSE);
		AppendMenu(SystemMenu_hMenu, MF_SEPARATOR, 0, 0);
		AppendMenu(SystemMenu_hMenu, MF_STRING, CONFIGMENU_MENU_ABOUT, "About");

		SendMessage(hWnd, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)gsTitleCaption);
		SendMessage(hWnd, WM_SETICON, TRUE, (LPARAM)LoadIcon(ghAppInstance, MAKEINTRESOURCE(IDI_MAIN)));

		hDriveInfo = GetDlgItem(hWnd, IDC_DRIVEINFO);
		hAllowCache = GetDlgItem(hWnd, IDC_ALLOWCACHE);
		hIOS = GetDlgItem(hWnd, IDC_IOS);
		hIterations = GetDlgItem(hWnd, IDC_ITERATIONS);
		hChunkSize = GetDlgItem(hWnd, IDC_CHUNKSIZE);
		hBytesPerIteration = GetDlgItem(hWnd, IDC_BYTESPERITERATION);
		hTotalDatasetSize = GetDlgItem(hWnd, IDC_TOTALDATASETSIZE);
		hProgress = GetDlgItem(hWnd, IDC_DSKPROGRESS);
		hStatus = GetDlgItem(hWnd, IDC_STATUS);

		hWriteGraph = GetDlgItem(hWnd, IDC_GRAPHWRITE);
		hWriteDiskTime = GetDlgItem(hWnd, IDC_DISKTIMEWRITE);
		hWriteLast = GetDlgItem(hWnd, IDC_LASTWRITE);
		hWriteAvg = GetDlgItem(hWnd, IDC_AVGWRITE);
		hWriteMin = GetDlgItem(hWnd, IDC_MINWRITE);
		hWriteMax = GetDlgItem(hWnd, IDC_MAXWRITE);
		hWriteIops = GetDlgItem(hWnd, IDC_IOPSWRITE);
		hWriteLatency = GetDlgItem(hWnd, IDC_LATWRITE);

		hReadGraph = GetDlgItem(hWnd, IDC_GRAPHREAD);
		hReadDiskTime = GetDlgItem(hWnd, IDC_DISKTIMEREAD);
		hReadLast = GetDlgItem(hWnd, IDC_LASTREAD);
		hReadAvg = GetDlgItem(hWnd, IDC_AVGREAD);
		hReadMin = GetDlgItem(hWnd, IDC_MINREAD);
		hReadMax = GetDlgItem(hWnd, IDC_MAXREAD);
		hReadIops = GetDlgItem(hWnd, IDC_IOPSREAD);
		hReadLatency = GetDlgItem(hWnd, IDC_LATREAD);

		//Large font for the headline throughput figures.
		HDC hDC = GetDC(hWnd);
		int iDpi = GetDeviceCaps(hDC, LOGPIXELSY);
		ReleaseDC(hWnd, hDC);

		ghBigFont = CreateFont(-MulDiv(16, iDpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, ANSI_CHARSET,
			OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");
		SendMessage(hWriteAvg, WM_SETFONT, (WPARAM)ghBigFont, TRUE);
		SendMessage(hReadAvg, WM_SETFONT, (WPARAM)ghBigFont, TRUE);

		int iGridSpacing = MulDiv(20, iDpi, 96);

		pWriteGraph = new CGraph(hWriteGraph, RGB(24, 27, 33), RGB(44, 50, 60), iGridSpacing, GRAPH_LEFT, true);
		pWriteGraph->SpacingTop(MulDiv(28, iDpi, 96));
		pWriteGraph->AddSeries(GRAPH_COLOR_MIN, 10.0, GraphTextFormatCallback);
		pWriteGraph->AddSeries(GRAPH_COLOR_MAX, 10.0, GraphTextFormatCallback);
		pWriteGraph->AddSeries(GRAPH_COLOR_AVG, 10.0, GraphTextFormatCallback);
		pWriteGraph->AddSeries(GRAPH_COLOR_LAST, 10.0, GraphTextFormatCallback);

		pReadGraph = new CGraph(hReadGraph, RGB(24, 27, 33), RGB(44, 50, 60), iGridSpacing, GRAPH_LEFT, true);
		pReadGraph->SpacingTop(MulDiv(28, iDpi, 96));
		pReadGraph->AddSeries(GRAPH_COLOR_MIN, 10.0, GraphTextFormatCallback);
		pReadGraph->AddSeries(GRAPH_COLOR_MAX, 10.0, GraphTextFormatCallback);
		pReadGraph->AddSeries(GRAPH_COLOR_AVG, 10.0, GraphTextFormatCallback);
		pReadGraph->AddSeries(GRAPH_COLOR_LAST, 10.0, GraphTextFormatCallback);

		ResetCounters();
		Set_Text(hDriveInfo, "n/a");
		Set_Text(hAllowCache, "n/a");
		Set_Text(hIOS, "n/a");
		Set_Text(hIterations, "n/a");
		Set_Text(hChunkSize, "n/a");
		Set_Text(hBytesPerIteration, "n/a");
		Set_Text(hTotalDatasetSize, "n/a");
		Set_Text(hStatus, "Ready.");
		EnableWindow(GetDlgItem(hWnd, IDC_CMDCOPY), FALSE);

		CenterWindow(hWnd);

		//Show the window first, then ask for the settings.
		PostMessage(hWnd, WM_APP_SHOWCONFIG, 0, 0);

		return TRUE; // Return TRUE to set the keyboard focus, Otherwise return FALSE.
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_APP_SHOWCONFIG)
	{
		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);
		PromptAndStart(hWnd);
		return TRUE;
	}

	//--------------------------------------------------------------------------

	if (uMsg == WM_COMMAND)
	{
		if (LOWORD(wParam) == ID_CMDMULTI) //- Run / Cancel Button.
		{
			if (!hThread)
			{
				PromptAndStart(hWnd);
			}
			else if (MessageBox(hMainDialog, "Are you sure you want to cancel the operation?", gsTitleCaption, MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES)
			{
				if (hThread)
				{
					gbCancel = true;
					EnableWindow(GetDlgItem(hMainDialog, ID_CMDMULTI), FALSE);
				}
			}
			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_CMDHELP)
		{
			OpenHelp(hWnd);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_CMDCOPY)
		{
			CopyResultsToClipboard(hWnd);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_ONTOP)
		{
			SetWindowPos(hWnd, IsDlgButtonChecked(hWnd, IDC_ONTOP) ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			return TRUE;
		}

		return FALSE;
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
		if (hThread)
		{
			MessageBox(hMainDialog, "Cancel the operation before closing.", gsTitleCaption, MB_ICONINFORMATION);
		}
		else {
			delete pWriteGraph;
			delete pReadGraph;

			if (ghBigFont)
			{
				DeleteObject(ghBigFont);
				ghBigFont = NULL;
			}

			EndDialog(hWnd, 0);

			hMainDialog = NULL;
		}

		return TRUE;
	}

	//--------------------------------------------------------------------------

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
