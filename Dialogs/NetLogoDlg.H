#ifndef _NETLOGODLG_H
#define _NETLOGODLG_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct _AboutDialogInfo {
	char* BuildDate;
	char* BuildTime;
	char* CopyRight;
	char* FileVersion;
	char* TitleCaption;
	HICON DisplayIcon;
	HWND OwnerHandle;
} ABOUTDLGINFO, * LPABOUTDLGINFO;

extern LPABOUTDLGINFO glpADI;

extern HWND NetLogoDialog_hWnd;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK NetLogoDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool IsNetLogoActive(void);
bool NetLogo(LPABOUTDLGINFO lpADI);
HWND NetLogoHandle(void);
void EndNetLogo(void);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
