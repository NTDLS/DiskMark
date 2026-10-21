#ifndef _MainDlg_CPP
#define _MainDlg_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>
#include <ShlObj.H>
#include <CommDlg.H>
#include <StdArg.H>

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

#pragma comment(lib, "comdlg32.lib")

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------(Variable Declarations)
HWND hMainDialog = NULL;

HWND hDriveInfo = NULL;
HWND hAllowCache = NULL;
HWND hFileSize = NULL;
HWND hDuration = NULL;
HWND hBlockSize = NULL;

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
int giBlockSize = 0;
int giDurationSeconds = 0;
__int64 giFileSize = 0;
bool gbUseWriteCache = false;

HANDLE hThread = NULL;
DWORD dwThreadID = 0;
volatile bool gbCancel = false;

//Results of the most recent test, for "Copy Results" and "Export".
BENCHCONFIG gBenchConfig;
BENCHSTATS gWriteStats;
BENCHSTATS gReadStats;
BENCHDRIVEINFO gDriveInfo;
SYSTEMTIME gTestStart;
char gsOutcome[32] = "";
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

	sprintf_s(sTemp, sizeof(sTemp), "%d s per phase", pConfig->DurationSeconds);
	Set_Text(hDuration, sTemp);

	FileSizeFriendly(pConfig->FileSize, sTemp, sizeof(sTemp));
	Set_Text(hFileSize, sTemp);

	FileSizeFriendly((__int64)pConfig->BlockSize, sTemp, sizeof(sTemp));
	Set_Text(hBlockSize, sTemp);
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

void BenchProgress(void* pContext, BenchEvent eEvent, const BENCHSTATS* pStats, int iElapsedMs, int iTotalMs)
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
	//The graph advances one pixel per update. Repeat each sample so the whole test spans the plot as a step chart.
	int iPixelsPerSample = pGraph->Width() / (gBenchConfig.DurationSeconds > 0 ? gBenchConfig.DurationSeconds : 1);
	if (iPixelsPerSample < 1)
	{
		iPixelsPerSample = 1;
	}
	for (int i = 0; i < iPixelsPerSample; i++)
	{
		pGraph->Update(dValues);
	}
	pGraph->Invalidate();

	sprintf_s(sTemp, sizeof(sTemp), "%s test: %d of %d s", bWrite ? "Write" : "Read",
		(int)(pStats->TotalSeconds + 0.5), gBenchConfig.DurationSeconds);
	Set_Text(hStatus, sTemp);

	SendMessage(hProgress, PBM_SETPOS, iElapsedMs, (LPARAM)0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI ThreadProc(LPVOID lpVoid)
{
	char sCaption[1024];
	sprintf_s(sCaption, sizeof(sCaption), "%s (%s)", gsTitleCaption, gsDrive);
	SendMessage(hMainDialog, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)sCaption);

	Set_Text(GetDlgItem(hMainDialog, ID_CMDMULTI), "Cancel");
	EnableWindow(GetDlgItem(hMainDialog, IDC_CMDCOPY), FALSE);
	EnableWindow(GetDlgItem(hMainDialog, IDC_CMDEXPORT), FALSE);

	memset(&gBenchConfig, 0, sizeof(gBenchConfig));
	strcpy_s(gBenchConfig.Drive, sizeof(gBenchConfig.Drive), gsDrive);
	gBenchConfig.BlockSize = (DWORD)giBlockSize;
	gBenchConfig.FileSize = giFileSize;
	gBenchConfig.DurationSeconds = giDurationSeconds;
	gBenchConfig.AllowWriteCache = gbUseWriteCache;

	gbHaveWriteStats = false;
	gbHaveReadStats = false;

	BenchGetDriveInfo(gBenchConfig.Drive, &gDriveInfo);
	GetLocalTime(&gTestStart);
	strcpy_s(gsOutcome, sizeof(gsOutcome), "Incomplete");

	ResetCounters();
	ShowConfiguration(&gBenchConfig);

	SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM)giDurationSeconds * 2 * 1000);

	char sError[1024];
	BenchResult eResult = RunDiskBench(&gBenchConfig, &gbCancel, BenchProgress, NULL, sError, sizeof(sError));

	if (eResult == BenchOk)
	{
		strcpy_s(gsOutcome, sizeof(gsOutcome), "Complete");
		char sTemp[256];
		sprintf_s(sTemp, sizeof(sTemp), "Complete. Write %.1f MB/s, Read %.1f MB/s (averages).",
			gWriteStats.AvgMBps, gReadStats.AvgMBps);
		Set_Text(hStatus, sTemp);
		SendMessage(hProgress, PBM_SETPOS, giDurationSeconds * 2 * 1000, (LPARAM)0);
	}
	else if (eResult == BenchCancelled)
	{
		strcpy_s(gsOutcome, sizeof(gsOutcome), "Cancelled (partial results)");
		Set_Text(hStatus, "Cancelled.");
	}
	else {
		strcpy_s(gsOutcome, sizeof(gsOutcome), "Failed (partial results)");
		Set_Text(hStatus, "Failed.");
		MessageBox(hMainDialog, sError, gsTitleCaption, MB_ICONERROR);
	}

	SendMessage(hMainDialog, (UINT)WM_SETTEXT, (WPARAM)0, (LPARAM)gsTitleCaption);

	gbCancel = false;
	Set_Text(GetDlgItem(hMainDialog, ID_CMDMULTI), "Run");
	EnableWindow(GetDlgItem(hMainDialog, ID_CMDMULTI), TRUE);
	EnableWindow(GetDlgItem(hMainDialog, IDC_CMDCOPY), gbHaveWriteStats || gbHaveReadStats);
	EnableWindow(GetDlgItem(hMainDialog, IDC_CMDEXPORT), gbHaveWriteStats || gbHaveReadStats);

	CloseHandle(hThread);
	hThread = NULL;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Appends formatted text to a growing report buffer.
static void AppendReport(char* sReport, size_t iReportSz, const char* sFormat, ...)
{
	size_t iLen = strlen(sReport);
	va_list args;
	va_start(args, sFormat);
	vsprintf_s(sReport + iLen, iReportSz - iLen, sFormat, args);
	va_end(args);
}

static void FormatCapacity(unsigned __int64 u64Bytes, char* sOut, size_t iOutSz)
{
	FileSizeFriendly((__int64)u64Bytes, sOut, (int)iOutSz);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BuildTextReport(char* sReport, size_t iReportSz)
{
	char sTotal[64], sFree[64], sBlock[64], sFile[64];
	FormatCapacity(gDriveInfo.TotalBytes, sTotal, sizeof(sTotal));
	FormatCapacity(gDriveInfo.FreeBytes, sFree, sizeof(sFree));
	FileSizeFriendly((__int64)gBenchConfig.BlockSize, sBlock, sizeof(sBlock));
	FileSizeFriendly(gBenchConfig.FileSize, sFile, sizeof(sFile));

	sReport[0] = '\0';
	AppendReport(sReport, iReportSz, "%s %s results\r\n", gsTitleCaption, gsFileVersion);
	AppendReport(sReport, iReportSz, "Date:           %04d-%02d-%02d %02d:%02d:%02d\r\n", gTestStart.wYear, gTestStart.wMonth,
		gTestStart.wDay, gTestStart.wHour, gTestStart.wMinute, gTestStart.wSecond);
	AppendReport(sReport, iReportSz, "Outcome:        %s\r\n\r\n", gsOutcome);

	AppendReport(sReport, iReportSz, "Drive\r\n");
	AppendReport(sReport, iReportSz, "  Drive:        %s (%s)\r\n", gDriveInfo.Root, gDriveInfo.DriveType);
	AppendReport(sReport, iReportSz, "  Volume:       %s\r\n", gDriveInfo.VolumeLabel[0] ? gDriveInfo.VolumeLabel : "(none)");
	AppendReport(sReport, iReportSz, "  File system:  %s\r\n", gDriveInfo.FileSystem);
	AppendReport(sReport, iReportSz, "  Device:       %s\r\n", gDriveInfo.Device);
	AppendReport(sReport, iReportSz, "  Bus / media:  %s / %s\r\n", gDriveInfo.Bus, gDriveInfo.Media);
	AppendReport(sReport, iReportSz, "  Capacity:     %s (%s free at start)\r\n\r\n", sTotal, sFree);

	AppendReport(sReport, iReportSz, "Configuration\r\n");
	AppendReport(sReport, iReportSz, "  Block size:      %s\r\n", sBlock);
	AppendReport(sReport, iReportSz, "  Test file size:  %s\r\n", sFile);
	AppendReport(sReport, iReportSz, "  Duration:        %d s per phase (sampled every second)\r\n", gBenchConfig.DurationSeconds);
	AppendReport(sReport, iReportSz, "  Write cache:     %s\r\n\r\n", gBenchConfig.AllowWriteCache ? "allowed (flush timed)" : "off (write-through)");

	AppendReport(sReport, iReportSz, "Results (MB/s, 1 MB = 1,048,576 bytes)\r\n");
	AppendReport(sReport, iReportSz, "          Average       Min       Max      Last      IOPS   Latency  Samples  Disk time\r\n");

	const BENCHSTATS* pStats[2] = { &gWriteStats, &gReadStats };
	const bool bHave[2] = { gbHaveWriteStats, gbHaveReadStats };
	const char* sPhase[2] = { "Write", "Read" };

	for (int i = 0; i < 2; i++)
	{
		if (!bHave[i])
		{
			continue;
		}

		char sLatency[64];
		FormatLatency(pStats[i]->AvgLatencyMs, sLatency, sizeof(sLatency));
		AppendReport(sReport, iReportSz, "  %-6s %9.1f %9.1f %9.1f %9.1f %9.0f %9s %8d %9.2f s\r\n", sPhase[i],
			pStats[i]->AvgMBps, pStats[i]->MinMBps, pStats[i]->MaxMBps, pStats[i]->LastMBps,
			pStats[i]->Iops, sLatency, pStats[i]->Samples, pStats[i]->TotalSeconds);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// One row per phase so several exports can be appended into one spreadsheet.
void BuildCsvReport(char* sReport, size_t iReportSz)
{
	sReport[0] = '\0';
	AppendReport(sReport, iReportSz,
		"Date,Outcome,Drive,DriveType,Volume,FileSystem,Device,Bus,Media,CapacityBytes,FreeBytes,"
		"BlockSizeBytes,TestFileBytes,DurationSeconds,WriteCache,Phase,AvgMBps,MinMBps,MaxMBps,LastMBps,"
		"IOPS,AvgLatencyMs,Samples,TotalSeconds,TotalBytes\r\n");

	const BENCHSTATS* pStats[2] = { &gWriteStats, &gReadStats };
	const bool bHave[2] = { gbHaveWriteStats, gbHaveReadStats };
	const char* sPhase[2] = { "Write", "Read" };

	for (int i = 0; i < 2; i++)
	{
		if (!bHave[i])
		{
			continue;
		}

		AppendReport(sReport, iReportSz,
			"%04d-%02d-%02d %02d:%02d:%02d,%s,\"%s\",%s,\"%s\",%s,\"%s\",%s,%s,%I64u,%I64u,%lu,%I64d,%d,%s,%s,"
			"%.2f,%.2f,%.2f,%.2f,%.0f,%.4f,%d,%.3f,%I64u\r\n",
			gTestStart.wYear, gTestStart.wMonth, gTestStart.wDay, gTestStart.wHour, gTestStart.wMinute, gTestStart.wSecond,
			gsOutcome, gDriveInfo.Root, gDriveInfo.DriveType, gDriveInfo.VolumeLabel, gDriveInfo.FileSystem,
			gDriveInfo.Device, gDriveInfo.Bus, gDriveInfo.Media, gDriveInfo.TotalBytes, gDriveInfo.FreeBytes,
			gBenchConfig.BlockSize, gBenchConfig.FileSize, gBenchConfig.DurationSeconds,
			gBenchConfig.AllowWriteCache ? "On" : "Off", sPhase[i],
			pStats[i]->AvgMBps, pStats[i]->MinMBps, pStats[i]->MaxMBps, pStats[i]->LastMBps,
			pStats[i]->Iops, pStats[i]->AvgLatencyMs, pStats[i]->Samples, pStats[i]->TotalSeconds, pStats[i]->TotalBytes);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CopyResultsToClipboard(HWND hWnd)
{
	char sReport[8192];
	BuildTextReport(sReport, sizeof(sReport));

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

void ExportResults(HWND hWnd)
{
	char sFileName[MAX_PATH];
	sprintf_s(sFileName, sizeof(sFileName), "DiskMark-%c-%04d%02d%02d-%02d%02d%02d", gDriveInfo.Root[0],
		gTestStart.wYear, gTestStart.wMonth, gTestStart.wDay, gTestStart.wHour, gTestStart.wMinute, gTestStart.wSecond);

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Text report (*.txt)\0*.txt\0CSV spreadsheet (*.csv)\0*.csv\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = sFileName;
	ofn.nMaxFile = sizeof(sFileName);
	ofn.lpstrDefExt = "txt";
	ofn.lpstrTitle = "Export Results";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetSaveFileName(&ofn))
	{
		return;
	}

	// Honor an explicit .csv extension even if the filter says text, and vice versa.
	const char* sExt = strrchr(sFileName, '.');
	bool bCsv = sExt ? (_stricmp(sExt, ".csv") == 0) : (ofn.nFilterIndex == 2);

	char sReport[8192];
	if (bCsv)
	{
		BuildCsvReport(sReport, sizeof(sReport));
	}
	else {
		BuildTextReport(sReport, sizeof(sReport));
	}

	HANDLE hFile = CreateFile(sFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD dwWritten = 0;
	if (hFile == INVALID_HANDLE_VALUE || !WriteFile(hFile, sReport, (DWORD)strlen(sReport), &dwWritten, NULL))
	{
		MessageBox(hWnd, "Failed to write the export file.", gsTitleCaption, MB_ICONERROR);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);
		}
		return;
	}
	CloseHandle(hFile);

	char sStatus[MAX_PATH + 32];
	sprintf_s(sStatus, sizeof(sStatus), "Exported to %s", sFileName);
	Set_Text(hStatus, sStatus);
}

////////

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
		hFileSize = GetDlgItem(hWnd, IDC_FILESIZE);
		hDuration = GetDlgItem(hWnd, IDC_DURATION);
		hBlockSize = GetDlgItem(hWnd, IDC_BLOCKSIZE);
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
		Set_Text(hFileSize, "n/a");
		Set_Text(hDuration, "n/a");
		Set_Text(hBlockSize, "n/a");
		Set_Text(hStatus, "Ready.");
		EnableWindow(GetDlgItem(hWnd, IDC_CMDCOPY), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_CMDEXPORT), FALSE);

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
		else if (LOWORD(wParam) == IDC_CMDABOUT)
		{
			ShowAbout(hWnd);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_CMDHELP)
		{
			OpenHelp(hWnd);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_CMDEXPORT)
		{
			ExportResults(hWnd);
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
			ShowAbout(hWnd);
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
