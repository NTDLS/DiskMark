#ifndef _MainDlg_H
#define _MainDlg_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------(Variable Declarations)
extern HWND hMainDialog;

extern char gsDrive[MAX_PATH];
extern int giBlockSize;	// Block size in bytes.
extern int giDurationSeconds;	// Seconds to run each phase (write, then read).
extern __int64 giFileSize;	// Test file size in bytes.
extern bool gbUseWriteCache;

extern HANDLE hThread;
extern DWORD dwThreadID;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//---------------------(Function Prototypes)
INT_PTR CALLBACK MainDialog(HWND hHwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI ThreadProc(LPVOID lpVoid);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
