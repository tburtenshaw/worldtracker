#include <time.h>
#include <windows.h>
#include <windowsx.h>
#include "mytrips.h"
#include "worldtracker.h"
#include "wtgraphs.h"
#include "wt_messages.h"

#undef HANDLE_WM_CONTEXTMENU
#define HANDLE_WM_CONTEXTMENU(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), 0L)


extern WORLDREGION *regionFirst;
extern LOCATIONHISTORY locationHistory;

extern COLOUR cDaySwatch[7];
extern COLOUR cMonthSwatch[12];
extern COLOUR cBlack;
extern COLOUR cWhite;
extern COLOUR cRed;

int RecalculateData(GRAPHINFO * gi)
{

//this should be under function FitEntireTime
		gi->fromtimestamp=gi->locationHistory->earliesttimestamp;
		printf("\nETS: %i", gi->fromtimestamp);
		gi->totimestamp=gi->locationHistory->latesttimestamp;



	printf("\ngraphtype %i", gi->graphType);
	if (gi->graphType & WT_GRAPHTYPE_STAY)	{
		printf("\nstay type");
		if (gi->stay)	{
			FreeLinkedListOfStays(gi->stay);
		}
		gi->stay = CreateStayListFromNSWE(&gi->region->nswe, gi->locationHistory);
	}
	else if (gi->graphType & WT_GRAPHTYPE_TRIP)	{
		printf("\n trip type");
		if (gi->trip)	{//first free memory of old trip
			FreeLinkedListOfTrips(gi->trip);
		}

		printf("\n recalc graph");
		gi->trip = GetLinkedListOfTrips(regionFirst, gi->locationHistory);
	}
	return 0;
}

int DrawLineGraph(GRAPHINFO *gi)
{
	int i;
	double *xarray;
	double *yarray;
	double xmin, ymin, xmax, ymax;

	unsigned long tsrange;
	double tsdivision;	//divided by the number of samples we want.
	double tsmark;	//where we'll next stop

	LOCATION *loc;

	int samplenumber;



	xmax=0;xmin=0;
	ymin=0;ymax=0;
	samplenumber=2000;
	xarray = malloc(sizeof(double)*samplenumber);
	yarray = malloc(sizeof(double)*samplenumber);

	//We want to sample 2000 points
	tsrange = gi->locationHistory->latesttimestamp - gi->locationHistory->earliesttimestamp;
	tsdivision = tsrange/samplenumber;
	tsmark = gi->locationHistory->earliesttimestamp + tsdivision;

	loc = gi->locationHistory->first;

	i=0;
	xarray[0]=0;	yarray[0]=0;
	while (loc)	{
		if (loc->timestampS > tsmark)	{	//if we're past the next division
			tsmark+=tsdivision;
			i++;
			if (i >= samplenumber)	{	//the very last might skip to an out-of-bounds, so we'll reduce it
				i--;
			}
			xarray[i]=0; yarray[i]=0;
		}
		if (xarray[i]==0)	{xarray[i]=loc->timestampS;}

		//if (yarray[i]==0)	{yarray[i]=	MetersApartFlatEarth(-36.8485,174.7633, loc->latitude,loc->longitude);}
		if (yarray[i]==0)	{yarray[i]=	MetersApartHaversine(-36.8485,174.7633, loc->latitude,loc->longitude);}

		//if (yarray[i]==0)	{yarray[i]=loc->latitude;}
		//if (yarray[i]==0)	{yarray[i]=loc->longitude;}
		//if (yarray[i]==0)	{yarray[i]=loc->distancefromprev/loc->secondsfromprev;}

		loc=loc->next;
	}
	samplenumber = i+1;	//as we subtract one in the loop to avoid out-of-bounds
	printf("We have %i points.", samplenumber);


	GraphLine(&gi->bmGraph, &cWhite, xmin, ymin, xmax, ymax, &cRed,samplenumber, xarray, yarray);	//make background

	free(xarray);
	free(yarray);
	return 1;
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
		printf("\ntriptype");
		trip = gi->trip;
		stay = NULL;
	}
	else	{
		printf("\nstaytype");
		stay = gi->stay;
		trip = NULL;
	}

	//background the same no matter the type
	GraphScatter(&gi->bmGraph, &cWhite, 0,0,0,0, 0, 0,NULL, NULL, NULL, NULL, NULL, NULL,0, 0, NULL, NULL);	//make background
	printf("\nBackground");


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
					xmax = 60*60*24*7-60*60;
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
			printf("%f %f\t",xdata,ydata);

		}

		trip=trip->next;
	}

	if (stay)	{
		printf("\nstay");
		STAY * workingStay;
		unsigned long timestamp, timestampend;
		unsigned long tsstart, tsfinish;

		long s;

		int	bucketnumber=0;
		int allocatedbuckets=3600;
		int numberofbuckets=0;
		int *bucket;

		tsstart = gi->fromtimestamp;
		tsfinish = gi->totimestamp;

		xmin=tsstart;
		xmax=tsfinish;
		ymin=0;
		ymajorunit=1;
		xmajorunit=24*60*60*7;
		ylabelfn=NULL;

		printf("graphing stay");
		bucket=malloc(sizeof(int)*allocatedbuckets);
		bucket[0]=0;	//set the first bucket empty
		memset(bucket,0,sizeof(int)*24);

		for (timestamp = tsstart; timestamp<tsfinish;)	{
			localtime_s(&timestamp, &time);

			//SET STARTING PERIOD
			if (gi->xAxisSeries == WT_SERIES_DAY) 	{	//set to the start of a day
				time.tm_hour=0;
				time.tm_min=0;
				time.tm_sec=0;
				timestamp = mktime(&time);
			}
			else if (gi->xAxisSeries == WT_SERIES_MONTH)	{
				time.tm_hour=0;
				time.tm_min=0;
				time.tm_sec=0;
				time.tm_mday=1;
				timestamp = mktime(&time);
			}
			else if (gi->xAxisSeries == WT_SERIES_TIMEOFDAY)	{
				time.tm_min=0;
				time.tm_sec=0;
				timestamp = mktime(&time);
			}
			else if (gi->xAxisSeries == WT_SERIES_WEEKDAY)	{
				time.tm_hour=0;
				time.tm_min=0;
				time.tm_sec=0;
				timestamp = mktime(&time);
			}



			//SET COLOUR
			//if we want to colour by day of week
			pointColour.R = cDaySwatch[time.tm_wday].R;
			pointColour.G = cDaySwatch[time.tm_wday].G;
			pointColour.B = cDaySwatch[time.tm_wday].B;
			pointColour.A = 200;

			//SET END PERIOD
			if (gi->xAxisSeries == WT_SERIES_DAY) 	{	//get the next day
				time.tm_mday++;
			}
			else if (gi->xAxisSeries == WT_SERIES_MONTH)	{
				time.tm_mon++;
			}
			else if (gi->xAxisSeries == WT_SERIES_TIMEOFDAY)	{
				bucketnumber=time.tm_hour;
				time.tm_hour++;
			}
			else if (gi->xAxisSeries == WT_SERIES_WEEKDAY)	{
				bucketnumber=time.tm_wday;
				time.tm_mday++;
			}



			//time.tm_isdst = -1;	??do i need this

			timestampend=mktime(&time);
			workingStay=stay;
			s =  SecondsInStay(workingStay, timestamp, timestampend);
			bucket[bucketnumber]+=s;
			xdata=timestamp;
			ydata = s;

			//CHOSE THE UNITS WE'LL DISPLAY IN
			if (gi->yAxisSeries == WT_SERIES_DUR_HOURS)	{
				ydata /=3600;
			}
			else if (gi->yAxisSeries == WT_SERIES_DUR_MINUTES)	{
				ydata /=60;
				ymax=24*60;
			}
			else if (gi->yAxisSeries == WT_SERIES_DUR_DAYS)	{
				ydata /=(3600*24);
				ymax=31;
			}

			//Get the units
			if ((gi->yAxisSeries == WT_SERIES_DUR_HOURS) && (gi->yAxisSeries == WT_SERIES_DAY))	//hours per day
				ymax=24;

			//printf("\n%i %f %f", timestamp, xdata,ydata);
			if (ydata>0)
				GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, NULL, NULL, NULL, NULL, NULL, &pointColour,5, 1, &xdata, &ydata);

			bucketnumber++;
			if (bucketnumber>numberofbuckets)
				numberofbuckets=bucketnumber;
			if (bucketnumber>allocatedbuckets)	{	//if we need more buckets
				printf("%i ", bucketnumber);
				allocatedbuckets+=3600;
				bucket = realloc(bucket,3600*sizeof(int));
			}
			//bucket[bucketnumber]=0;	//set the next bucket to empty
			timestamp=timestampend;

		}
		//Now go through the buckets
		ymax=0;
		for (int i=0;i<numberofbuckets;i++)	{
			if (ymax<bucket[i])	ymax=bucket[i];
			printf("\nbucket: %i:\tseconds: %i", i, bucket[i]);
		}

		for (int i=0;i<numberofbuckets;i++)	{
			xmax=numberofbuckets; xmin=0;
			xdata=i;
			ydata=bucket[i];
			ymin=0;
			GraphScatter(&gi->bmGraph, NULL, xmin, ymin, xmax, ymax, xmajorunit, ymajorunit, NULL, NULL, NULL, NULL, NULL, &pointColour,5, 1, &xdata, &ydata);
			printf("\nbucket: %i:\tseconds: %i", i, bucket[i]);
		}


		//Free the buckets
		free(bucket);

	}


	//draw axis
	printf("\nxmin %f, ymin %f, xmax %f, ymax %f, xmajorunit %f, ymajorunit %f",xmin, ymin, xmax, ymax, xmajorunit, ymajorunit);
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
		info->region = regionFirst;

		info->graphType = WT_GRAPHTYPE_SCATTER|WT_GRAPHTYPE_TRIP;

		info->colourSeries =WT_SERIES_MONTH;
		info->colourSeries =WT_SERIES_WEEKDAY;
		info->colourSeries =WT_SERIES_DIRECTION;

		info->xAxisSeries =WT_SERIES_TIMESTAMP;
		info->xAxisSeries =WT_SERIES_TIMEOFDAY;
		info->xAxisSeries =WT_SERIES_WEEKDAY;


		info->fromtimestamp=info->locationHistory->earliesttimestamp;
		printf("\nETS: %i", info->fromtimestamp);
		info->totimestamp=info->locationHistory->latesttimestamp;

		info->graphType = WT_GRAPHTYPE_SCATTER|WT_GRAPHTYPE_TRIP;
		info->xAxisSeries = WT_SERIES_MONTH;
		//info->yAxisSeries = WT_SERIES_DUR_DAYS;
		info->colourSeries =WT_SERIES_WEEKDAY;

		info->xAxisSeries = WT_SERIES_TIMEOFDAY;
		//info->xAxisSeries =WT_SERIES_WEEKDAY;
		info->xAxisSeries = WT_SERIES_MONTH;//WT_SERIES_DAY;//WT_SERIES_TIMESTAMP;

		info->graphType = WT_GRAPHTYPE_LINE;

		break;
	case WM_PAINT:
		printf("paint");
		PaintMainGraph(hwnd, info);
		break;
	case WM_LBUTTONDOWN:
		//if (info->colourSeries == WT_SERIES_DIRECTION)
//			SendMessage(hwnd, WT_WM_GRAPH_SETSERIESCOLOR, WT_SERIES_WEEKDAY,0);
//		else
//			SendMessage(hwnd, WT_WM_GRAPH_SETSERIESCOLOR, WT_SERIES_DIRECTION,0);


		SendMessage(hwnd, WT_WM_GRAPH_RECALCDATA, 0,0);
		SendMessage(hwnd, WT_WM_GRAPH_REDRAW, 0,0);
		InvalidateRect(hwnd, NULL, FALSE);
		break;
	case WT_WM_GRAPH_SETSERIESCOLOR:
		info->colourSeries = wParam;
		break;

	case WT_WM_GRAPH_SETREGION:
		info->region  =(WORLDREGION *)wParam;
		break;
	case WT_WM_GRAPH_RECALCDATA:
		RecalculateData(info);
		break;
	case WT_WM_GRAPH_REDRAW:
		hdc = GetDC(hwnd);
		printf("redraw");
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
		printf("no hbmgraph");
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
	printf("blt");
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
	else if (gi->graphType & WT_GRAPHTYPE_LINE)	{
		DrawLineGraph(gi);
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
//		ExportTripData(graphInfo->trip, "histogram.csv");
		printf("\nexport");
//		FreeLinkedListOfTrips(trip);
//		ExportTimeInNSWE(&regionAway.nswe,1336910400, 1452834934, 24*60*60, &locationHistory);

}
