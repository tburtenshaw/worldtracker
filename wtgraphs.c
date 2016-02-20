#include <time.h>
#include <windows.h>
#include <windowsx.h>
#include "mytrips.h"
#include "worldtracker.h"
#include "wtgraphs.h"

#undef HANDLE_WM_CONTEXTMENU
#define HANDLE_WM_CONTEXTMENU(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), 0L)


#define WT_WM_GRAPH_REDRAW (WM_APP+100)
#define WT_WM_GRAPH_RECALCDATA (WM_APP+101)

#define WT_WM_GRAPH_SETSERIESCOLOR (WM_APP+102)
#define WT_WM_GRAPH_SETSERIESX (WM_APP+103)
#define WT_WM_GRAPH_SETSERIESY (WM_APP+104)

extern WORLDREGION regionHome;
extern WORLDREGION regionAway;
extern WORLDREGION *pRegionFirstExcluded;
extern LOCATIONHISTORY locationHistory;

extern COLOUR cDaySwatch[7];
extern COLOUR cMonthSwatch[12];
extern COLOUR cBlack;
extern COLOUR cWhite;

int RecalculateData(GRAPHINFO * gi)
{
	printf("\ngraphtype %i", gi->graphType);
	if (gi->graphType & WT_GRAPHTYPE_STAY)	{
		printf("\nstay type");
		gi->stay = CreateStayListFromNSWE(&gi->region->nswe, gi->locationHistory);
	}
	else if (gi->graphType & WT_GRAPHTYPE_TRIP)	{
		printf("\n trip type");
		if (gi->trip)	{//first free memory of old trip
			FreeLinkedListOfTrips(gi->trip);
		}

		printf("\n recalc graph");
		gi->trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, gi->locationHistory);
	}
	return 0;
}

int DrawScatterGraph(GRAPHINFO *gi)
{
	struct tm time;

	double xdata;
	double ydata;
	double xmin, ymin, xmax, ymax;
	double xmajorunit, ymajorunit;
	COLOUR pointColour;
	void * xlabelfn;
	void * ylabelfn;

	int graphType;	//this is a reduced option version of the graphinfo structure. (i.e. it can only be WT_GRAPHTYPE_TRIP or WT_GRAPHTYPE_STAY)
	TRIP * trip;
	STAY * stay;


	//Make graphtype one or the other
	if (gi->graphType & WT_GRAPHTYPE_TRIP)	{
		graphType = WT_GRAPHTYPE_TRIP;
	}
	else
		graphType = WT_GRAPHTYPE_STAY;


	//then depending on graphtype we'll go through a different linked list
	if (graphType == WT_GRAPHTYPE_TRIP)	{
		trip = gi->trip;
		stay = NULL;
	}
	else	{
		stay = gi->stay;
		trip = NULL;
	}

	//background the same no matter the type
	GraphScatter(&gi->bmGraph, &cWhite, 0,0,0,0, 0, 0,NULL, NULL, NULL, NULL, NULL, NULL,0, 0, NULL, NULL);	//make background



	while (trip)	{
		xdata=trip->leavetime;
		ydata=trip->leavetime - trip->arrivetime;
		//printf("%i %i\n", trip->leavetime, trip->leavetime - trip->arrivetime );
		if (trip->leavetime > gi->fromtimestamp)	{
			ylabelfn = labelfnMinutesFromSeconds;
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

			}

			//plot the point
			GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, NULL, NULL, NULL, NULL, NULL, &pointColour,5, 1, &xdata, &ydata);

		}

		trip=trip->next;
	}

	if (stay)	{
		STAY * workingStay;
		unsigned long timestamp, timestampend;
		unsigned long tsstart, tsfinish;

		long s;

		tsstart = gi->fromtimestamp;
		tsfinish = gi->totimestamp;

		xmin=tsstart;
		xmax=tsfinish;
		ymin=0;
		ymax=0;
		ymajorunit=1;
		xmajorunit=24*60*60*30;
		ylabelfn=NULL;

		printf("graphing stay");
		for (timestamp = tsstart; timestamp<tsfinish;)	{
			localtime_s(&timestamp, &time);

			//if (gi->xAxisSeries == WT_SERIES_DAY) 	{
				time.tm_hour=0;
				time.tm_min=0;
				time.tm_sec=0;
				timestamp = mktime(&time);
//			}


					pointColour.R = cDaySwatch[time.tm_wday].R;
					pointColour.G = cDaySwatch[time.tm_wday].G;
					pointColour.B = cDaySwatch[time.tm_wday].B;
					pointColour.A = 200;

			time.tm_mday++;
			//time.tm_isdst = -1;

			timestampend=mktime(&time);
			workingStay=stay;
			s =  SecondsInStay(workingStay, timestamp,timestampend);
			xdata=timestamp;
			ydata = s;
			ydata /=3600;

			if (ceil(ydata)>ymax)	ymax=ceil(ydata);

//			printf("\n%i %f %f", timestamp, xdata,ydata);
			if (ydata>0)
				GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, NULL, NULL, NULL, NULL, NULL, &pointColour,5, 1, &xdata, &ydata);

			timestamp=timestampend;


		}



	}


	//draw axis
	GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, &cBlack, NULL, NULL, xlabelfn, ylabelfn, NULL,5, 1, NULL, NULL);


	return 0;
}


LRESULT CALLBACK GraphWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	GRAPHINFO * info;
	HDC hdc;

	info = (void*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (msg) {
	case WM_CREATE:
		info = calloc(sizeof(GRAPHINFO),1);
		SetWindowLong(hwnd, GWL_USERDATA, (long)info);
		info->hbmGraph = NULL;
		info->locationHistory= &locationHistory;
		info->region = &regionAway;

		info->graphType = WT_GRAPHTYPE_SCATTER|WT_GRAPHTYPE_TRIP;
		info->graphType = WT_GRAPHTYPE_SCATTER|WT_GRAPHTYPE_STAY;



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
		if (info->colourSeries == WT_SERIES_DIRECTION)
			SendMessage(hwnd, WT_WM_GRAPH_SETSERIESCOLOR, WT_SERIES_WEEKDAY,0);
		else
			SendMessage(hwnd, WT_WM_GRAPH_SETSERIESCOLOR, WT_SERIES_DIRECTION,0);


		SendMessage(hwnd, WT_WM_GRAPH_RECALCDATA, 0,0);
		SendMessage(hwnd, WT_WM_GRAPH_REDRAW, 0,0);
		break;
	case WT_WM_GRAPH_SETSERIESCOLOR:
		info->colourSeries = wParam;
		break;

	case WT_WM_GRAPH_RECALCDATA:
		RecalculateData(info);
		break;
	case WT_WM_GRAPH_REDRAW:
		hdc = GetDC(hwnd);
		info->hbmGraph = MakeHBitmapGraph(hwnd, hdc, info);
		ReleaseDC(hwnd, hdc);
		InvalidateRect(hwnd, NULL, 0);
		break;
	case WM_CONTEXTMENU:
		HANDLE_WM_CONTEXTMENU(hwnd, wParam, lParam, GraphContextMenu);
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

	if (gi->hbmGraph !=NULL)	{
		DeleteObject(gi->hbmGraph);
	}


	GetClientRect(hwnd, &rect);
	width=rect.right-rect.left;
	height =rect.bottom-rect.top;

	//for passing through to the bitmapinit
	tempOptions.height=height;
	tempOptions.width=width;

	bitmapInit(&gi->bmGraph, &tempOptions, gi->locationHistory);

	//Draw the graph depending on settings
 	if (gi->graphType & WT_GRAPHTYPE_SCATTER)
		DrawScatterGraph(gi);
	else if (gi->graphType & WT_GRAPHTYPE_SCATTER)	{
		1;
	}



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

void GraphContextMenu(HWND hwnd, HWND hwndContext, int xPos, int yPos)
{
	GRAPHINFO * graphInfo;
	graphInfo = (void*)GetWindowLong(hwnd, GWL_USERDATA);

//		TRIP * trip;
//		trip = GetLinkedListOfTrips(&regionHome.nswe, &regionAway.nswe, pRegionFirstExcluded, &locationHistory);
		ExportTripData(graphInfo->trip, "histogram.csv");
		printf("\nexport");
//		FreeLinkedListOfTrips(trip);
//		ExportTimeInNSWE(&regionAway.nswe,1336910400, 1452834934, 24*60*60, &locationHistory);

}
