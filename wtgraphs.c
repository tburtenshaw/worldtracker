#include <time.h>
#include <windows.h>
#include "mytrips.h"
#include "worldtracker.h"
#include "wtgraphs.h"

#define WT_WM_GRAPH_REDRAW WM_APP+100
#define WT_WM_GRAPH_SETSERIESCOLOR WM_APP+101
#define WT_WM_GRAPH_SETSERIESX WM_APP+102
#define WT_WM_GRAPH_SETSERIESY WM_APP+103

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
		info = calloc(sizeof(GRAPHINFO),0);
		SetWindowLong(hwnd, GWL_USERDATA, (long)info);
		info->hbmGraph = NULL;
		info->locationHistory= &locationHistory;


		info->graphType = WT_GRAPHTYPE_SCATTER|WT_GRAPHTYPE_TRIP;

		info->colourSeries =WT_SERIES_MONTH;
		info->colourSeries =WT_SERIES_WEEKDAY;
		info->colourSeries =WT_SERIES_DIRECTION;

		info->xAxisSeries =WT_SERIES_TIMESTAMP;
		info->xAxisSeries =WT_SERIES_TIMEOFDAY;
		info->xAxisSeries =WT_SERIES_WEEKDAY;


		info->fromtimestamp=1336938800;	//default
		info->totimestamp=1452834934;
		break;
	case WM_PAINT:
		PaintMainGraph(hwnd, info);
		break;
	case WM_LBUTTONDOWN:
		SendMessage(hwnd, WT_WM_GRAPH_REDRAW, 0,0);
		break;
	case WT_WM_GRAPH_REDRAW:
		hdc = GetDC(hwnd);
		info->hbmGraph = MakeHBitmapGraph(hwnd, hdc, info);
		ReleaseDC(hwnd, hdc);
		InvalidateRect(hwnd, NULL, 0);
		break;
	case WM_RBUTTONDOWN:
		TRIP * trip;
		trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, &locationHistory);
		ExportTripData(trip, "histogram.csv");
		FreeLinkedListOfTrips(trip);


	ExportTimeInNSWE(&regionAway.nswe,1336910400, 1452834934, 24*60*60, &locationHistory);

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

HBITMAP MakeHBitmapGraph(HWND hwnd, HDC hdc, GRAPHINFO * gi)
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
	void * xlabelfn;
	void * ylabelfn;

	TRIP * trip;
	trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, gi->locationHistory);


	bitmapInit(&gi->bmGraph, &tempOptions, gi->locationHistory);

	GraphScatter(&gi->bmGraph, &cWhite, 0,0,0,0, 0, 0,NULL, NULL, NULL, NULL, NULL, NULL,0, 0, NULL, NULL);	//make background
	while (trip)	{
		xdata=trip->leavetime;
		ydata=trip->leavetime - trip->arrivetime;
		//printf("%i %i\n", trip->leavetime, trip->leavetime - trip->arrivetime );
		if (trip->leavetime > gi->fromtimestamp)	{

			xmin = gi->fromtimestamp;
			ymin = 0;
			xmax = gi->totimestamp;
			ymax = 3600;
			xmajorunit = 60*60*24*7;
			ymajorunit = 60*5;
			xlabelfn = NULL;
			ylabelfn = NULL;

			switch (gi->xAxisSeries)	{
				case WT_SERIES_TIMEOFDAY:
					int secondssincemidnight;
					localtime_s(&trip->leavetime, &time);
					secondssincemidnight = time.tm_hour*3600 + time.tm_min*60 + time.tm_sec - time.tm_isdst*3600;
					xmin=0;
					xmax = 60*60*24;
					xmajorunit=60*60;
					xdata = secondssincemidnight;
					xlabelfn=labelfnTimeOfDayFromSeconds;
					break;
				case WT_SERIES_WEEKDAY:
					long secondspastsunday;
					localtime_s(&trip->leavetime, &time);
					secondspastsunday = time.tm_wday*3600*24 + time.tm_hour*3600 + time.tm_min*60 + time.tm_sec;
					xmin=0;
					xmax = 60*60*24*7;
					xmajorunit=60*60*24;
					xdata = secondspastsunday;
					xlabelfn=labelfnShortDayOfWeekFromSeconds;
					break;
				case	WT_SERIES_TIMESTAMP:
					xmin = gi->fromtimestamp;
					ymin = 0;
					xmax = gi->totimestamp;
					ymax = 3600;
					xmajorunit = 60*60*24*7;
					ymajorunit = 60*5;
					xlabelfn = NULL;
					break;


			}

			switch (gi->colourSeries)	{
				case WT_SERIES_DIRECTION:
					if (trip->direction==-1)	{
						pointColour.R=255;
						pointColour.G=0;
						pointColour.B=0;
						pointColour.A=100;
					}
					if (trip->direction==1)	{
						pointColour.R=0;
						pointColour.G=145;
						pointColour.B=0;
						pointColour.A=100;
					}
				break;
				case WT_SERIES_WEEKDAY:
					localtime_s(&trip->leavetime, &time);
					//printf("%i %i",time.tm_wday);
					pointColour.R = cDaySwatch[time.tm_wday].R;
					pointColour.G = cDaySwatch[time.tm_wday].G;
					pointColour.B = cDaySwatch[time.tm_wday].B;
					pointColour.A = 120;
				break;
				case WT_SERIES_MONTH:
					localtime_s(&trip->leavetime, &time);
					//printf("%i %i",time.tm_wday);
					pointColour.R = cMonthSwatch[time.tm_mon].R;
					pointColour.G = cMonthSwatch[time.tm_mon].G;
					pointColour.B = cMonthSwatch[time.tm_mon].B;
					pointColour.A = 120;
				break;


				break;
			}

			//plot the point
			GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, NULL, NULL, NULL, NULL, NULL, &pointColour,5, 1, &xdata, &ydata);

		}

		trip=trip->next;
	}

	ylabelfn = labelfnMinutesFromSeconds;
	//draw axis
	GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, &cBlack, NULL, NULL, xlabelfn, ylabelfn, NULL,5, 1, NULL, NULL);
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

