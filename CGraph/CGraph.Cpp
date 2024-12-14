///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Copyright © NetworkDLS 2002, All rights reserved
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
// PARTICULAR PURPOSE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>
#include <CommCtrl.H>

#include "CGraph.H"
#include "../NSWFL/NSWFL.H"

using namespace NSWFL::Conversion;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT NewGraphProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CGraph *lpG = (CGraph *) GetProp(hWnd, "Class");
	if(lpG)
	{
		if(lpG->Settings.IsInitialized)
		{
			//--------------------------------------------------------------------------
			if(uMsg == WM_PAINT)
			{
				HDC hGraphDC;
				PAINTSTRUCT psGraph;
				if((hGraphDC = BeginPaint(hWnd, &psGraph)))
				{
					lpG->Redraw(hGraphDC);
					EndPaint(hWnd, &psGraph);
				}
			}

			return CallWindowProc(lpG->_OldGraphProc, hWnd, uMsg, wParam, lParam); 
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGraph::CGraph()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGraph::CGraph(HWND hWnd, DWORD dwINBGColor, DWORD dwINGridColor, short iINGridSpacing, short iINDirection, bool bUniformScaling)
{
	this->Initialize(hWnd, dwINBGColor, dwINGridColor, iINGridSpacing, iINDirection, bUniformScaling);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGraph::~CGraph()
{
	this->Destroy();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Redraw(void)
{
	if(this->TryLock())
	{
		HDC hGraphDC = GetDC(this->Config.hWnd);
		this->Redraw(hGraphDC);
		ReleaseDC(this->Config.hWnd, hGraphDC);

		this->UnLock();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::WriteText(GRAPHTEXTATTRIBUTES *pGTA)
{
	int iYPixels = GetDeviceCaps(pGTA->DC, LOGPIXELSY);
	int iFontHeight = -MulDiv(pGTA->FontSize, iYPixels, 72);

	HFONT hFont = CreateFont(
		iFontHeight,
		0,0,0,
		FW_NORMAL,
		0,0,0,
		ANSI_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		FF_DONTCARE,
		pGTA->FontName
	);

	SelectObject(pGTA->DC, hFont);

	SetTextColor(pGTA->DC, pGTA->FGColor);
	SetBkMode(pGTA->DC, pGTA->BGMode);
	SetBkColor(pGTA->DC, pGTA->BGColor);
	TextOut(pGTA->DC, pGTA->x, pGTA->y, pGTA->Text, (int)strlen(pGTA->Text));

	DeleteObject(hFont);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Redraw(HDC hGraphDC)
{
	if(!this->TryLock())
	{
		return;
	}

	RECT rc;
    HPEN hPen1;
    HBITMAP MemBM;
    HBITMAP OldBM;
    HGDIOBJ GdiObj;
    int x;
    long y;

	memset(&rc, 0, sizeof(rc));
	rc.left = this->Config.Width;
	rc.bottom = this->Config.Height;

    HBRUSH hBGBrush = CreateSolidBrush(this->Settings.BGColor);

    hPen1 = CreatePen(PS_SOLID, 0, this->Settings.GridColor);

    HDC MemDC = CreateCompatibleDC(hGraphDC);

    //Create a bitmap big enough for the client rectangle
    MemBM = CreateCompatibleBitmap(hGraphDC, this->Config.Width, this->Config.Height);
    OldBM = (HBITMAP) SelectObject(MemDC, MemBM);

    // Draw the Grid
    FillRect(MemDC, &rc, hBGBrush);

    SelectObject(MemDC, hPen1);

    for(x = this->Config.GridOffset; x <= this->Config.Width; x = x + this->Settings.SpacingX)
    {
        MoveToEx(MemDC, x, 0, NULL);
        LineTo(MemDC, x, this->Config.Height);
    }

    for(y = 0; y <= this->Config.Height; y = y + this->Settings.SpacingY)
    {
        MoveToEx(MemDC, 0, y, NULL);
        LineTo(MemDC, this->Config.Width, y);
    }

    // Now to draw the graph in one pass
	for(short iSeries = 0; iSeries < this->Config.SeriesCount; iSeries++)
	{
		//Draw lines:
		HPEN hPen2 = CreatePen(PS_SOLID, 0, this->Series[iSeries].LineColor);
		SelectObject(MemDC, hPen2);
	    Polyline(MemDC, this->Series[iSeries].pPoints, this->Config.Width + 1);
	    DeleteObject(hPen2);
	}

	GRAPHTEXTATTRIBUTES gta;
	memset(&gta, 0, sizeof(gta));

	if(this->Settings.Direction == GRAPH_LEFT)
	{
		gta.x = this->Config.Width;
	}
	else {
		gta.x = 1;
	}
	gta.y = 1;
	gta.DC = MemDC;
	gta.FGColor = this->Settings.BGColor;
	gta.BGMode = OPAQUE;
	gta.FontSize = 6;
	strcpy_s(gta.FontName, sizeof(gta.FontName), "Courier");

	//Write Text
	for(short iSeries = 0; iSeries < this->Config.SeriesCount; iSeries++)
	{
		if(this->Series[iSeries].pTextFormatCallback)
		{
			gta.BGColor = this->Series[iSeries].LineColor;

			if(((GraphTextFormatCallback)this->Series[iSeries].pTextFormatCallback)(this, &this->Series[iSeries], &gta))
			{
				int iYPixels = GetDeviceCaps(gta.DC, LOGPIXELSY);
				int iFontHeight = -MulDiv(gta.FontSize, iYPixels, 72);

				HFONT hFont = CreateFont(
					iFontHeight,
					0,0,0,
					FW_NORMAL,
					0,0,0,
					ANSI_CHARSET,
					OUT_TT_PRECIS,
					CLIP_DEFAULT_PRECIS,
					ANTIALIASED_QUALITY,
					FF_DONTCARE,
					gta.FontName
				);

				SelectObject(gta.DC, hFont);

				SIZE FontSize;
				GetTextExtentPoint32(gta.DC, gta.Text, (int)strlen(gta.Text), &FontSize);

				if(this->Settings.Direction == GRAPH_LEFT)
				{
					gta.x -= FontSize.cx + 3;
				}

				SetTextColor(gta.DC, gta.FGColor);
				SetBkMode(gta.DC, gta.BGMode);
				SetBkColor(gta.DC, gta.BGColor);
				TextOut(gta.DC, gta.x, gta.y, gta.Text, (int)strlen(gta.Text));

				if(this->Settings.Direction == GRAPH_RIGHT)
				{
					gta.x += FontSize.cx + 3;
				}

				DeleteObject(hFont);
			}
		}
	}

    BitBlt(hGraphDC, 0, 0, this->Config.Width, this->Config.Height, MemDC, 0, 0, SRCCOPY);

    GdiObj = SelectObject(MemDC, OldBM);

    // Clean up after ourselves to prevent resource leaks.
    DeleteDC(MemDC);
    DeleteObject(hBGBrush);
    DeleteObject(hPen1);
    DeleteObject(OldBM);
    DeleteObject(MemBM);
    DeleteObject(GdiObj);

	this->UnLock();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::AddSeries(DWORD dwINLineColor, double dINMinimumRange)
{
	this->AddSeries(dwINLineColor, dINMinimumRange, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::AddSeries(DWORD dwINLineColor, double dINMinimumRange, GraphTextFormatCallback pTextFormatCallback)
{
	this->Lock();

	if(dINMinimumRange < 1)
	{
		dINMinimumRange = 1; //The minimum cannot be zero.
	}

	int iMemory = 0;

	this->Series = (GRAPHSERIES *) realloc(this->Series, (this->Config.SeriesCount + 1) *  sizeof(GRAPHSERIES));

	memset(&this->Series[this->Config.SeriesCount], 0, sizeof(GRAPHSERIES));

	this->Series[this->Config.SeriesCount].pTextFormatCallback = pTextFormatCallback;
	this->Series[this->Config.SeriesCount].MinimumRange = dINMinimumRange;
	this->Series[this->Config.SeriesCount].LineColor = dwINLineColor;
	this->Series[this->Config.SeriesCount].CurrentScale = 1;
	this->Series[this->Config.SeriesCount].Index = this->Config.SeriesCount;

	this->Series[this->Config.SeriesCount].pPoints = (POINT *) calloc(this->Config.Width + 1, sizeof(POINT));
	this->Series[this->Config.SeriesCount].Values = (double *) calloc(this->Config.Width + 1, sizeof(double));

	for(int i = 0; i <= this->Config.Width; i++)
	{
		this->Series[this->Config.SeriesCount].pPoints[i].x = i;
		this->Series[this->Config.SeriesCount].pPoints[i].y = this->Config.Height;
	}

	if(dINMinimumRange > this->Stats.MinimumRange)
	{
		this->Stats.MinimumRange = dINMinimumRange;
	}

	this->Config.SeriesCount++;

	this->UnLock();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Initialize(HWND hWnd, DWORD dwINBGColor, DWORD dwINGridColor, short iINGridSpacing, short iINDirection, bool bUniformScaling)
{
	this->Initialize(hWnd, dwINBGColor, dwINGridColor, iINGridSpacing, iINDirection, bUniformScaling, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Initialize(HWND hWnd, DWORD dwINBGColor, DWORD dwINGridColor,
		short iINGridSpacing, short iINDirection, bool bUniformScaling, bool bInitCS)
{
	if(bInitCS)
	{
		InitializeCriticalSection(&this->csLock);
		SetProp(hWnd, "Class", this);
	}

	this->_OldGraphProc = (WNDPROC) SetWindowLongPtr(hWnd, GWLP_WNDPROC, (INT_PTR)&NewGraphProc);

    RECT rc;
	memset(&rc, 0, sizeof(rc));
    GetClientRect(hWnd, &rc);

	this->Series = NULL;
	this->Points = NULL;

	memset(&this->Stats, 0, sizeof(this->Stats));

	memset(&this->Config, 0, sizeof(this->Config));
	this->Config.hWnd        = hWnd;
	this->Config.GridOffset  = 0;
	this->Config.SeriesCount = 0;
	this->Config.Height      = rc.bottom-rc.top;
	this->Config.Width       = rc.right-rc.left;

	memset(&this->Settings, 0, sizeof(this->Settings));
	this->Settings.SpacingX       = iINGridSpacing;
    this->Settings.SpacingY       = iINGridSpacing;
	this->Settings.BGColor        = dwINBGColor;
    this->Settings.GridColor      = dwINGridColor;
    this->Settings.IsFrozen       = false;
    this->Settings.Direction      = iINDirection;
	this->Settings.UniformScaling = bUniformScaling;
	this->Settings.IsInitialized  = true;

	int iMemory = (this->Config.Width + 1) * sizeof(POINT);
    this->Points = (POINT *) calloc( iMemory, 1);
    memset(Points, 0, iMemory);

    ShowWindow(this->Config.hWnd, SW_SHOW);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGraph::Update(double dValue)
{
	this->Lock();

	if(this->Settings.IsFrozen)
    {
		this->UnLock();
		return false;
    }

	this->UnLock();

	SendMessage(this->Config.hWnd, (UINT)WM_SETREDRAW , (WPARAM)FALSE, (LPARAM)0);

	this->Lock();

	bool bResult = false;

	if(this->Config.SeriesCount == 1)
	{
		if(this->Settings.Direction == GRAPH_LEFT)
		{
			this->Config.GridOffset++;
		}
		else {
			this->Config.GridOffset--;
		}

		bResult = this->Update(0, dValue);

		if(this->Config.GridOffset == this->Settings.SpacingX || this->Config.GridOffset == -this->Settings.SpacingX)
		{
			this->Config.GridOffset = 0;
		}	
	}

	SendMessage(this->Config.hWnd, (UINT)WM_SETREDRAW , (WPARAM)TRUE, (LPARAM)0);

	this->UnLock();

	return bResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGraph::Update(double *dValues)
{
	this->Lock();

	if(this->Settings.IsFrozen)
    {
		this->UnLock();
		return false;
    }

	this->UnLock();

	SendMessage(this->Config.hWnd, (UINT)WM_SETREDRAW , (WPARAM)FALSE, (LPARAM)0);

	this->Lock();

	bool bResult = false;

	if(this->Settings.Direction == GRAPH_LEFT)
    {
		this->Config.GridOffset++;
    }
    else {
		this->Config.GridOffset--;
    }

	for(short iSeries = 0; iSeries < this->Config.SeriesCount; iSeries++)
	{
		bResult = this->Update(iSeries, dValues[iSeries]);
	}

	if(this->Config.GridOffset == this->Settings.SpacingX || this->Config.GridOffset == -this->Settings.SpacingX)
	{
		this->Config.GridOffset = 0;
	}

	this->UnLock();

	SendMessage(this->Config.hWnd, (UINT)WM_SETREDRAW , (WPARAM)TRUE, (LPARAM)0);

	return bResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGraph::Update(short iSeries, double dValue)
{
	this->Lock();

    if(this->Settings.IsFrozen)
    {
		this->UnLock();
		return false;
    }

	this->Series[iSeries].LastValue = dValue;

	this->Stats.TopValue = this->Series[iSeries].MinimumRange;
	this->Stats.HighestValue = dValue;
	this->Series[iSeries].TopValue = dValue;

	if(dValue > this->Stats.TopValue)
	{
		this->Stats.TopValue = dValue;
	}

	for(short iS = 0; iS < this->Config.SeriesCount; iS++)
	{
		this->Series[iS].TopValue = this->Stats.TopValue;
		this->Series[iS].HighestValue = dValue;

		if(iS == iSeries && dValue > this->Series[iS].TopValue)
		{
			this->Series[iS].TopValue = dValue;
			this->Series[iS].HighestValue = dValue;
		}

		for(int i = 0; i < this->Config.Width; i++)
		{
			if(this->Series[iS].Values[i] > this->Stats.TopValue)
			{
				this->Series[iS].TopValue = this->Series[iS].Values[i];
				this->Stats.TopValue = this->Series[iS].Values[i];
			}
			if(this->Series[iS].Values[i] > this->Stats.HighestValue)
			{
				this->Series[iS].HighestValue = this->Series[iS].Values[i];
				this->Stats.HighestValue = this->Series[iS].Values[i];
			}
			if(this->Series[iS].Values[i] > this->Series[iS].TopValue)
			{
				this->Series[iS].TopValue = this->Series[iS].Values[i];
			}
			if(this->Series[iS].Values[i] > this->Series[iS].HighestValue)
			{
				this->Series[iS].HighestValue = this->Series[iS].Values[i];
			}
		}
	}

	double dHeight = (this->Config.Height - this->Settings.SpacingTop) - this->Settings.SpacingBottom;

	if(this->Settings.UniformScaling)
	{
		this->Series[iSeries].CurrentScale = (this->Stats.MinimumRange) / ((double)this->Stats.TopValue);
	}
	else {
		this->Series[iSeries].CurrentScale = (this->Series[iSeries].MinimumRange) / ((double)this->Series[iSeries].TopValue);
	}

	//Save values.
	for(int i = this->Config.Width; i > 0; i--)
	{
		this->Series[iSeries].Values[i] = this->Series[iSeries].Values[i-1];
	}
	this->Series[iSeries].Values[0] = dValue;
	
	if(this->Settings.Direction == GRAPH_LEFT)
    {
		if(this->Settings.UniformScaling)
		{
			Points[0].y = (int) ((dHeight - ((dHeight * (dValue * this->Series[iSeries].CurrentScale)) / this->Stats.MinimumRange)) + this->Settings.SpacingTop);

			//Perform scaling.
			for(int i = 0; i <= this->Config.Width; i++)
			{
	            this->Series[iSeries].pPoints[i].y = (int)
					((dHeight - ((dHeight * (this->Series[iSeries].Values[i] * this->Series[iSeries].CurrentScale)) / this->Stats.MinimumRange)) + this->Settings.SpacingTop);
			}
		}
		else {
			Points[0].y = (int) ((dHeight - ((dHeight * (dValue * this->Series[iSeries].CurrentScale)) / this->Series[iSeries].MinimumRange)) + this->Settings.SpacingTop);

			//Perform scaling.
			for(int i = 0; i <= this->Config.Width; i++)
			{
	            this->Series[iSeries].pPoints[i].y = (int)
					((dHeight - ((dHeight * (this->Series[iSeries].Values[i] * this->Series[iSeries].CurrentScale)) / this->Series[iSeries].MinimumRange)) + this->Settings.SpacingTop);
			}
		}

		for(int i = 1; i <= this->Config.Width; i++)
		{
			Points[i].y = this->Series[iSeries].pPoints[i-1].y;
		}

		for(int i = 0; i <= this->Config.Width; i++)
		{
			this->Series[iSeries].pPoints[i].y = Points[i].y;
		}
    }
    else {
		if(this->Settings.UniformScaling)
		{
			Points[this->Config.Width].y = (int) ((dHeight - ((dHeight * (dValue * this->Series[iSeries].CurrentScale)) / this->Stats.MinimumRange)) + this->Settings.SpacingTop);

			//Perform scaling.
			for(int i = 1; i <= this->Config.Width; i++)
			{
	            this->Series[iSeries].pPoints[this->Config.Width - i].y = (int)
					((dHeight - ((dHeight * (this->Series[iSeries].Values[i] * this->Series[iSeries].CurrentScale)) / this->Stats.MinimumRange)) + this->Settings.SpacingTop);
			}
		}
		else {
			Points[this->Config.Width].y = (int) ((dHeight - ((dHeight * (dValue * this->Series[iSeries].CurrentScale)) / this->Series[iSeries].MinimumRange)) + this->Settings.SpacingTop);

			//Perform scaling.
			for(int i = 1; i <= this->Config.Width; i++)
			{
	            this->Series[iSeries].pPoints[this->Config.Width - i].y = (int)
					((dHeight - ((dHeight * (this->Series[iSeries].Values[i] * this->Series[iSeries].CurrentScale)) / this->Series[iSeries].MinimumRange)) + this->Settings.SpacingTop);
			}
		}

		for(int i = this->Config.Width - 1; i >= 0; i--)
        {
            Points[i].y = this->Series[iSeries].pPoints[i+1].y;
        }

        for(int i = this->Config.Width + 1; i >= 0; i--)
        {
            this->Series[iSeries].pPoints[i].y = Points[i].y;
        }
    }

	this->UnLock();

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Destroy(void)
{
	this->Destroy(true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Destroy(bool bDeleteCS)
{
	if(this->Settings.IsInitialized)
	{
		this->Lock();

		this->Settings.IsInitialized = false;

		SetWindowLongPtr(this->Config.hWnd, GWLP_WNDPROC, (INT_PTR)this->_OldGraphProc);
		SendMessage(this->Config.hWnd, WM_PAINT, 0, 0);

		for(short iSeries = 0; iSeries < this->Config.SeriesCount; iSeries++)
		{
			free(this->Series[iSeries].pPoints);
			free(this->Series[iSeries].Values);
		}
		free(this->Series);
		free(this->Points);

		this->Series = NULL;
		this->Points = NULL;

		memset(&this->Stats, 0, sizeof(this->Stats));
		memset(&this->Config, 0, sizeof(this->Config));
		memset(&this->Settings, 0, sizeof(this->Settings));

		this->UnLock();

		if(bDeleteCS)
		{
			RemoveProp(this->Config.hWnd, "Class");
			DeleteCriticalSection(&this->csLock);
			this->Config.hWnd = NULL;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Freeze(bool bFreeze)
{
	this->Settings.IsFrozen = bFreeze;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGraph::IsFrozen()
{
    return this->Settings.IsFrozen;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Invalidate(void)
{
	RedrawWindow(this->Config.hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGraph::TryLock(void)
{
	return (TryEnterCriticalSection(&this->csLock) > 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Lock(void)
{
	EnterCriticalSection(&this->csLock);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::UnLock(void)
{
	LeaveCriticalSection(&this->csLock);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::Reset(void)
{
	if(this->Config.hWnd)
	{
		this->Lock();

		int ilSeries = this->Config.SeriesCount;

		GRAPHSERIES *lpSeries = (GRAPHSERIES *) calloc(ilSeries, sizeof(GRAPHSERIES));
		if(lpSeries)
		{
			//Make a temp copy of the series collection.
			memcpy_s(lpSeries, sizeof(GRAPHSERIES) * ilSeries, this->Series, sizeof(GRAPHSERIES) * ilSeries);

			GRAPHSETTINGS lSettings;
			memcpy_s(&lSettings, sizeof(lSettings), &this->Settings, sizeof(this->Settings));

			GRAPHCONFIG lConfig;
			memcpy_s(&lConfig, sizeof(lConfig), &this->Config, sizeof(this->Config));

			this->Destroy(false);

			this->Initialize(lConfig.hWnd, lSettings.BGColor,
				lSettings.GridColor, lSettings.SpacingX, lSettings.Direction, this->Settings.UniformScaling, false);

			for(short iSeries = 0; iSeries < ilSeries; iSeries++)
			{
				this->AddSeries(lpSeries[iSeries].LineColor, lpSeries[iSeries].MinimumRange,
					(GraphTextFormatCallback)lpSeries[iSeries].pTextFormatCallback);
			}

			memcpy_s(&this->Settings, sizeof(this->Settings), &lSettings, sizeof(lSettings));
			memcpy_s(&this->Config, sizeof(this->Config), &lConfig, sizeof(lConfig));

			free(lpSeries);
			this->Redraw();
		}
		this->UnLock();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	This function should not be used as it is incomplete (and buggy).
*/
/*
void CGraph::Resize(int iX, int iY, int iInWidth, int iInHeight)
{
	this->Lock();

	MoveWindow(this->Config.hWnd, iX, iY, iInWidth, iInHeight, TRUE);

	this->Config.Height = iInHeight;
	this->Config.Width = iInWidth;

	int iMemory = (this->Config.Width + 1) * sizeof(POINT);
    this->Points = (POINT *) realloc(this->Points, iMemory);

	for(short iSeries = 0; iSeries < this->Config.SeriesCount; iSeries++)
	{
		this->pSeries[iSeries].pPoints = (POINT *) realloc(this->pSeries[iSeries].pPoints, iMemory);

		if(this->Settings.Direction == GRAPH_LEFT)
		{
			for(int i = 0; i <= this->Config.Width; i++)
			{
				this->pSeries[iSeries].pPoints[i].x = i;
				this->pSeries[iSeries].pPoints[i].y = this->Config.Height;
			}
		}
		else if(this->Settings.Direction == GRAPH_RIGHT)
		{
			for(int i = this->Config.Width; i >= 0; i--)
			{
				this->pSeries[iSeries].pPoints[i].x = i;
				this->pSeries[iSeries].pPoints[i].y = this->Config.Height;
			}
		}

	}

	this->UnLock();
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CGraph::Width(void)
{
	return this->Config.Width;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CGraph::Height(void)
{
	return this->Config.Height;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CGraph::SeriesCount(void)
{
	return this->Config.SeriesCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CGraph::GridOffset(void)
{
	return this->Config.GridOffset;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HWND CGraph::hWnd(void)
{
	return this->Config.hWnd;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGraph::UniformScaling(void)
{
	return this->Settings.UniformScaling;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CGraph::SpacingTop(void)
{
	return this->Settings.SpacingTop;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::SpacingTop(int Pixles)
{
	this->Settings.SpacingTop = Pixles;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CGraph::SpacingBottom(void)
{
	return this->Settings.SpacingBottom;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CGraph::SpacingBottom(int Pixles)
{
	this->Settings.SpacingBottom = Pixles;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
