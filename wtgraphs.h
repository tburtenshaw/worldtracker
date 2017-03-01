//This does the windows part of the graph display
//Much of the bit-work happens in the shared library which can graph for the command line
//#include <windows.h>
//#include "mytrips.h"
//#include "worldtracker.h"

#define WT_GRAPH_DEFAULT -1

#define WT_GRAPHTYPE_SCATTER 0x01
#define WT_GRAPHTYPE_HISTOGRAM 0x02
#define WT_GRAPHTYPE_LINE 0x04

#define WT_GRAPHTYPE_LOCATION 0x10	//data is based on a location
#define WT_GRAPHTYPE_TRIP 0x20		//data is based on a list of trips (TRIP)
#define WT_GRAPHTYPE_STAY 0x40		//based on a STAY struct


//firstly allow to set that we're looking at a duration (e.g. of a trip or a stay)
//all of these are a length of time, probably more likely used on a y-axis.
#define	WT_SERIES_DUR_SECONDS 1
#define	WT_SERIES_DUR_MINUTES 2
#define	WT_SERIES_DUR_HOURS 3
#define	WT_SERIES_DUR_DAYS 4
#define	WT_SERIES_DUR_WEEKS 5
#define	WT_SERIES_DUR_MONTHS 6
#define	WT_SERIES_DUR_YEARS 7

#define	WT_SERIES_TIMEOFDAY 8	//from 0000 to 23:59
#define	WT_SERIES_WEEKDAY 9
#define	WT_SERIES_MONTH 10
#define WT_SERIES_TIMESTAMP 11
#define WT_SERIES_DAY 12
#define WT_SERIES_DIRECTION 13
#define WT_SERIES_LATITUDE 14
#define WT_SERIES_LONGITUDE 15
#define WT_SERIES_DISTANCE 16
#define WT_SERIES_SPEED 16

typedef struct sGraphInfo GRAPHINFO;

struct sGraphInfo	{
	HBITMAP hbmGraph;
	BM	bmGraph;

	LOCATIONHISTORY *locationHistory;
	WORLDREGION * region;
	STAY * stay;	//associated stay
	TRIP * trip;

	int graphType;
	int xAxisSeries;	//whether this uses month, or accurancy, or time
	int yAxisSeries;
	int colourSeries;

	int markersize;

	unsigned long	fromtimestamp;
	unsigned long	totimestamp;

	double xmin;
	double xmax;

	double ymin;
	double ymax;

	double xmajorunit;
	double ymajorunit;
};



LRESULT CALLBACK GraphWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
int PaintMainGraph(HWND hwnd, GRAPHINFO* gi);
HBITMAP MakeHBitmapGraph(HWND hwnd, HDC hdc, GRAPHINFO * gi);
int RecalculateData(GRAPHINFO * gi);

int DrawScatterGraph(GRAPHINFO *gi);
int DrawLineGraph(GRAPHINFO *gi);

void GraphWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
void GraphContextMenu(HWND hwnd, HWND hwndContext, int xPos, int yPos);
