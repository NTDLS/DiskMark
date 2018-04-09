#ifndef _MainDlg_H
#define _MainDlg_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------(Constant Definitions)
#define ONEMEGABYTE	1048576.0f

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------(Variable Declarations)
extern HWND hMainDialog;

extern HWND hAllowCache;
extern HWND hRounds;
extern HWND hRuns;
extern HWND hSetSize;
extern HWND hPerSetSize;
extern HWND hTotalSetSize;

extern HWND hWriteDiskTime;
extern HWND hWriteLast;
extern HWND hWriteAvg;
extern HWND hWriteMin;
extern HWND hWriteMax;

extern HWND hReadDiskTime;
extern HWND hReadLast;
extern HWND hReadAvg;
extern HWND hReadMin;
extern HWND hReadMax;

extern char gsDrive[MAX_PATH];
extern int giBufferSize;
extern int giRuns;
extern int giRounds;
extern bool gbUseWriteCache;

extern HANDLE hThread;
extern DWORD dwThreadID;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------(Function Prototypes)
INT_PTR CALLBACK MainDialog(HWND hHwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI ThreadProc(LPVOID lpVoid);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
