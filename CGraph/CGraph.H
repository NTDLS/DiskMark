///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Copyright © NetworkDLS 2002, All rights reserved
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
// PARTICULAR PURPOSE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CGRAPH_H
#define _CGRAPH_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define GRAPH_LEFT  0
#define GRAPH_RIGHT 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct _tag_Graph_Settings {
	DWORD BGColor;
	DWORD GridColor;
	bool IsFrozen;
	short Direction;
	short SpacingX;
	short SpacingY;
	bool IsInitialized;
	bool UniformScaling;
	int SpacingTop;
	int SpacingBottom;
} GRAPHSETTINGS, * LPGRAPHSETTINGS;

typedef struct _tag_Graph_Config {
	long Height;
	long Width;
	short SeriesCount;
	short GridOffset;
	HWND hWnd;
} GRAPHCONFIG, * LPGRAPHCONFIG;

typedef struct _tag_Graph_Stats {
	double MinimumRange;			// The minimum value that the graph should scale to.
	double HighestValue;			// The highest value in the values array (*Values).
	double TopValue;				// The value represented at the top of the graph.
} GRAPHSTATS, * LPGRAPHSTATS;

typedef struct _tag_Text_Arrtibutes {
	HDC DC;
	char Text[255];
	int x;
	int y;
	DWORD BGColor;
	DWORD FGColor;
	int BGMode; //GDI Modes: TRANSPARENT or OPAQUE

	char FontName[255];
	int FontSize;
} GRAPHTEXTATTRIBUTES, * LPGRAPHTEXTATTRIBUTES;

typedef struct _tag_Series {
	short Index;					// The index of the series in the series array.
	double* Values;					// Array of floating point values that make up the graph.
	POINT* pPoints;					// The GDI points that will be drawn one the background.
	DWORD LineColor;				// The line-color for this series.
	double MinimumRange;			// The minimum value that the graph should scale to.
	double HighestValue;			// The highest value in the values array (*Values).
	double TopValue;				// The value represented at the top of the graph.
	double CurrentScale;			// Decimal value of the current graph scale (based on MinimumRange).
	double LastValue;				// The last value added to the values array (*Values).
	void* pTextFormatCallback;		// The text formatter callback for this series.
} GRAPHSERIES, * LPGRAPHSERIES;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CGraph {

public:
	typedef bool (*GraphTextFormatCallback)(CGraph* pGraph, GRAPHSERIES* pSeries, GRAPHTEXTATTRIBUTES* pTextAttributes);

	CGraph();
	~CGraph();
	CGraph(HWND hWnd, DWORD dwINBGColor, DWORD dwINGridColor, short iINGridSpacing, short iINDirection, bool bUniformScaling);

	GRAPHSETTINGS Settings;
	GRAPHSERIES* Series;

	void AddSeries(DWORD dwINLineColor, double dINMinimumRange);
	void AddSeries(DWORD dwINLineColor, double dINMinimumRange, GraphTextFormatCallback pGraphTextFormatCallback);

	void Initialize(HWND hWnd, DWORD dwINBGColor, DWORD dwINGridColor, short iINGridSpacing, short iINDirection, bool bUniformScaling);

	bool Update(double dValue);
	bool Update(double* dValues);
	bool Update(short iSeries, double dValue);

	//void Resize(int iX, int iY, int iInWidth, int iInHeight);
	void Redraw(HDC hGraphDC);
	void Redraw(void);
	void Destroy(void);
	void Freeze(bool bFreeze);
	bool IsFrozen(void);
	void Invalidate(void);
	void Reset(void);
	int Width(void);
	int Height(void);
	int SeriesCount(void);
	int GridOffset(void);
	HWND hWnd(void);
	bool UniformScaling(void);
	int SpacingTop(void);
	void SpacingTop(int Pixles);
	int SpacingBottom(void);
	void SpacingBottom(int Pixles);
	void WriteText(GRAPHTEXTATTRIBUTES* gta);

	void UnLock(void);
	bool TryLock(void);
	void Lock(void);

protected:
	WNDPROC _OldGraphProc;

	POINT* Points;
	GRAPHSTATS Stats;
	GRAPHCONFIG Config;

	friend LRESULT NewGraphProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void Initialize(HWND hWnd, DWORD dwINBGColor, DWORD dwINGridColor, short iINGridSpacing, short iINDirection, bool bUniformScaling, bool bInitCS);
	void Destroy(bool bDeleteCS);
	CRITICAL_SECTION csLock;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
