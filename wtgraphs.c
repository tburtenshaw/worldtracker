#include <windows.h>
#include "mytrips.h"
#include "worldtracker.h"
#include "wtgraphs.h"

extern WORLDREGION regionHome;
extern WORLDREGION regionAway;
extern WORLDREGION *pRegionFirstExcluded;
extern LOCATIONHISTORY locationHistory;

LRESULT CALLBACK GraphWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	GRAPHINFO * info;
	HDC hdc;

	info = (void*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (msg) {
	case WM_CREATE:
		info = malloc(sizeof(GRAPHINFO));
		SetWindowLong(hwnd, GWL_USERDATA, (long)info);
		info->hbmGraph = NULL;
		info->fromtimestamp=1336938800;	//default
		info->totimestamp=1452834934;
		break;
	case WM_PAINT:
		PaintMainGraph(hwnd, info);
		break;
	case WM_LBUTTONDOWN:
		hdc = GetDC(hwnd);
		info->hbmGraph = MakeHBitmapGraph(hwnd, hdc, info, &locationHistory);
		ReleaseDC(hwnd, hdc);
		InvalidateRect(hwnd, NULL, 0);

		break;
	case WM_RBUTTONDOWN:
		TRIP * trip;
		trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, &locationHistory);
		ExportTripData(trip, "histogram.csv");
		FreeLinkedListOfTrips(trip);
		break;


	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);


	}
	return 0;
}


int PaintMainGraph(HWND hwnd, GRAPHINFO * gi)
{
	HDC hdc;
	HDC memDC;
	PAINTSTRUCT ps;
	HGDIOBJ oldBitmap;

	RECT rect;
	int height, width;

	if (!gi->hbmGraph)	{
		hdc= BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		return 0;
	}


	GetClientRect(hwnd, &rect);
	width=rect.right-rect.left;
	height =rect.bottom-rect.top;


	hdc= BeginPaint(hwnd, &ps);
	memDC = CreateCompatibleDC(hdc);

	oldBitmap = SelectObject(memDC, gi->hbmGraph);
	BitBlt(hdc,0, 0, width, height, memDC, 0, 0, SRCCOPY);
	SelectObject(memDC, oldBitmap);


	DeleteDC(memDC);
	EndPaint(hwnd, &ps);
	return 0;
}

HBITMAP MakeHBitmapGraph(HWND hwnd, HDC hdc, GRAPHINFO * gi, LOCATIONHISTORY *lh)
{
	RECT rect;

	HBITMAP bitmap;
	int height, width;
	int x,y;
	OPTIONS tempOptions;

	BITMAPINFO bmi;


	BYTE * bits;
	COLOUR c;


	if (gi->hbmGraph !=NULL)	{
		DeleteObject(gi->hbmGraph);
	}


	GetClientRect(hwnd, &rect);
	width=rect.right-rect.left;
	height =rect.bottom-rect.top;

	//for passing through to the bitmapinit
	tempOptions.height=height;
	tempOptions.width=width;

	double xd[1];
	double yd[1];


	TRIP * trip;
	trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, &locationHistory);


	bitmapInit(&gi->bmGraph, &tempOptions, &locationHistory);

	GraphScatter(&gi->bmGraph, &cWhite, 0,0,0,0, 0, 0,NULL, NULL, NULL, NULL,0, 0, NULL, NULL);	//make background
	while (trip)	{
		xd[0]=trip->leavetime;
		yd[0]=trip->leavetime - trip->arrivetime;
		//printf("%i %i\n", trip->leavetime, trip->leavetime - trip->arrivetime );
		if (trip->leavetime > gi->fromtimestamp)	{
			if (trip->direction==-1)
			   GraphScatter(&gi->bmGraph, NULL, gi->fromtimestamp,0,gi->totimestamp,3600, 60*60*24*7, 100, &cBlack, NULL, NULL, &regionHome.baseColour,5, 1, xd, yd);
			if (trip->direction==1)
			   GraphScatter(&gi->bmGraph, NULL, gi->fromtimestamp,0,gi->totimestamp,3600, 60*60*24*7, 100, NULL, NULL, NULL, &regionAway.baseColour,5, 1, xd, yd);
		}

		trip=trip->next;
	}
	FreeLinkedListOfTrips(trip);




	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	GdiFlush();
	bitmap = CreateDIBSection(hdc, &bmi,DIB_RGB_COLORS, &bits, NULL, 0);
	//printf("create dib: %x", (unsigned long)bitmap);
	int b=0;
	for (y=0;y<height;y++)	{
		for (x=0;x<width;x++)	{
			c= bitmapPixelGet(&gi->bmGraph, x, y);
			bits[b] =c.B;	b++;
			bits[b] =c.G;	b++;
			bits[b] =c.R;	b++;
		}
		b=(b+3) & ~3;	//round to next WORD alignment at end of line
	}

	GdiFlush();

	bitmapDestroy(&gi->bmGraph);

	gi->hbmGraph=bitmap;
	return bitmap;
}

