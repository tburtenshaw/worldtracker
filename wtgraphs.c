#include <time.h>
#include <windows.h>
#include "mytrips.h"
#include "worldtracker.h"
#include "wtgraphs.h"

extern WORLDREGION regionHome;
extern WORLDREGION regionAway;
extern WORLDREGION *pRegionFirstExcluded;
extern LOCATIONHISTORY locationHistory;

extern COLOUR cDaySwatch[7];
extern COLOUR cMonthSwatch[12];
extern COLOUR cBlack;
extern COLOUR cWhite;


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

	struct tm time;

	if (gi->hbmGraph !=NULL)	{
		DeleteObject(gi->hbmGraph);
	}


	GetClientRect(hwnd, &rect);
	width=rect.right-rect.left;
	height =rect.bottom-rect.top;

	//for passing through to the bitmapinit
	tempOptions.height=height;
	tempOptions.width=width;



	double xdata;
	double ydata;
	double xmin, ymin, xmax, ymax;
	double xmajorunit, ymajorunit;
	COLOUR pointColour;

	TRIP * trip;
	trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, &locationHistory);


	bitmapInit(&gi->bmGraph, &tempOptions, &locationHistory);

	GraphScatter(&gi->bmGraph, &cWhite, 0,0,0,0, 0, 0,NULL, NULL, NULL, NULL,0, 0, NULL, NULL);	//make background
	while (trip)	{
		xdata=trip->leavetime;
		ydata=trip->leavetime - trip->arrivetime;
		//printf("%i %i\n", trip->leavetime, trip->leavetime - trip->arrivetime );
		if (trip->leavetime > gi->fromtimestamp)	{
			gi->colourSeries =WT_SERIES_WEEKDAY;
			gi->xAxisSeries =WT_SERIES_TIMEOFDAY;

			xmin = gi->fromtimestamp;
			ymin = 0;
			xmax = gi->totimestamp;
			ymax = 3600;
			xmajorunit = 60*60*24*7;
			ymajorunit = 60*5;


			switch (gi->xAxisSeries)	{
				case WT_SERIES_TIMEOFDAY:
					int secondssincemidnight;
					localtime_s(&trip->leavetime, &time);
					secondssincemidnight = time.tm_hour*3600 + time.tm_min*60 + time.tm_sec;
					xmin=0;
					xmax = 60*60*24;
					xmajorunit=60*60;
					xdata = secondssincemidnight;
					break;
			}

			switch (gi->colourSeries)	{
				case WT_SERIES_DIRECTION:
					if (trip->direction==-1)
						pointColour.R=255;
						pointColour.G=0;
						pointColour.B=0;
						pointColour.A=100;
					if (trip->direction==1)
						pointColour.R=0;
						pointColour.G=255;
						pointColour.B=0;
						pointColour.A=100;
				break;
				case WT_SERIES_WEEKDAY:
					localtime_s(&trip->leavetime, &time);
					//printf("%i %i",time.tm_wday);
					pointColour.R = cDaySwatch[time.tm_wday].R;
					pointColour.G = cDaySwatch[time.tm_wday].G;
					pointColour.B = cDaySwatch[time.tm_wday].B;
					pointColour.A = 120;
				break;
			}

			//plot the point
			GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, NULL, NULL, NULL, &pointColour,5, 1, &xdata, &ydata);

		}

		trip=trip->next;
	}

	//draw axis
	GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, &cBlack, NULL, NULL, NULL,5, 1, NULL, NULL);
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

