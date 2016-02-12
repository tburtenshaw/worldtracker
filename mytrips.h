/* This contains all the functions that are shared between the command line and Windows program */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct sRGBAColour COLOUR;
typedef struct sBitmap BM;
typedef struct sOptions OPTIONS;
typedef struct sLocation LOCATION;
typedef struct sLocationHistory LOCATIONHISTORY;
typedef struct sHeatmap HEATMAP;
typedef struct sNswe NSWE;
typedef struct sTrip TRIP;
typedef struct sWorldCoord WORLDCOORD;
typedef struct sWorldRegion WORLDREGION;	//this can be a linked list
typedef struct sPreset PRESET;

#define MAX_DIMENSION 4096*2
#define PI 3.14159265
#define EARTH_MEAN_RADIUS_KM 6371

#define COLOUR_BY_TIME 0
#define COLOUR_BY_SPEED 1
#define COLOUR_BY_ACCURACY 2
#define COLOUR_BY_DAYOFWEEK 3
#define COLOUR_BY_HOUR 4
#define COLOUR_BY_MONTH 5

#define MAX_COLOURBY_OPTION COLOUR_BY_MONTH

struct sRGBAColour	{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};

struct sHeatmap	{
	unsigned int * heatmappixels;
	int width;
	int height;
	unsigned int maxtemp;
	unsigned char *radius;
};

struct sNswe	{
	//order shouldn't matter as we're not directly accessing from memory, or saving to file
	double west;
	double east;
	double north;
	double south;

};

struct sWorldCoord	{
	double latitude;
	double longitude;
};

struct sOptions	{	//what we can get from the command line
	char *jsonfilenameinput;	//these are just pointers to either the default, or the command line argument
	char *pngfilenameinput;
	char *kmlfilenameinput;

	char jsonfilenamefinal[256];	//this actually holds the filename we'll use
	char pngfilenamefinal[256];	//this actually holds the filename we'll use
	char kmlfilenamefinal[256];	//this actually holds the filename we'll use

	char title[256];

	int width;
	int height;

	int forceheight;	//do we keep the height fixed when adjusting ratios? if 0 (default) we keep the width fixed

	NSWE nswe;

	double zoom;
	double aspectratio;

	unsigned long	fromtimestamp;
	unsigned long	totimestamp;

	unsigned char alpha;		//the alpha value of the line
	int thickness;		//the thickness of the line
	COLOUR gridcolour;
	int gridsize;

	int colourby;		//it'll be a COLOUR_BY_..., default COLOUR_BY_TIME
	long colourcycle;	//number of seconds before going red to red. Defaults to six months
	void* colourextra;
};


struct sBitmap	{
	int width;
	int height;

	double zoom;	//on a full map, this is the number of pixels per degree
	NSWE nswe;	//this should be copied from options, and the BM keeps its own

	OPTIONS *options;

	char *bitmap;	//always going to be a four channel RGBA bitmap now
	int sizebitmap;

	LOCATIONHISTORY *lh;	//pointer to the location history associated with this bitmap
	HEATMAP *heatmap;
	unsigned long countPoints;	//the number of points plotted (statistics)
};


struct sLocation	{
	double latitude;
	double longitude;
	long timestampS; //we'll use a long instead of the high precision of google
	int accuracy;

	double distancefromprev;
	long secondsfromprev;

	LOCATION* next;	//linked list
	LOCATION* prev;
	LOCATION* next1ppd;
	LOCATION* next10ppd;
	LOCATION* next100ppd;
	LOCATION* next1000ppd;
	LOCATION* next10000ppd;
};

struct sLocationHistory	{
	FILE *json;
	unsigned long	filesize;
	unsigned long	filepos;
	//char *jsonfilename;


	unsigned long	numPoints;
	unsigned long	earliesttimestamp;
	unsigned long	latesttimestamp;
	LOCATION * first;
	LOCATION * last;
};

struct sTrip	{
	TRIP * next;
	int	direction;	//1 is a to b, -1 is b to a, 0 isn't decided
	unsigned long	leavetime;
	unsigned long	arrivetime;
};


struct sWorldRegion	{
	NSWE nswe;
	COLOUR baseColour;
	WORLDREGION * next;	//next in the linked list
};

struct sPreset	{
	char * name;
	char * abbrev;
	NSWE nswe;
};

//int LoadLocations(LOCATIONHISTORY *locationHistory, char *jsonfilename);
//The progress function is called roughly 256 times, and returns a number roughly up to 256 or 257
//It can be Null, and it is ignored. It is NOT PRECISE.
int LoadLocations(LOCATIONHISTORY *locationHistory, char *jsonfilename, void(*progressfn)(int));
int FreeLocations(LOCATIONHISTORY *locationHistory);
int ReadLocation(LOCATIONHISTORY *lh, LOCATION *location);

void LoadPresets(PRESET *preset, int * pCount, int maxCount);
int NsweFromPreset(OPTIONS *options, char *lookuppreset, PRESET * presetarray, int numberofpresets);
char * SuggestAreaFromNSWE(NSWE* viewport, PRESET * presetarray, int numberofpresets);
int RationaliseOptions(OPTIONS *options);
int MakeProperFilename(char *targetstring, char *source, char *def, char *ext);

int WriteKMLFile(BM* bm);

int bitmapInit(BM* bm, OPTIONS* options, LOCATIONHISTORY *lh);
int bitmapPixelSet(BM* bm, int x, int y, COLOUR *c);
COLOUR bitmapPixelGet(BM* bm, int x, int y);
int bitmapFilledCircle(BM* bm, double x, double y, double radius, COLOUR *c);
int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, int thickness, COLOUR *c);
int bitmapCoordLine(BM *bm, double lat1, double lon1, double lat2, double lon2, int thickness, COLOUR *c);
int bitmapSquare(BM* bm, int x0, int y0, int x1, int y1, COLOUR *cBorder, COLOUR *cFill);

int bitmapWrite(BM* bm, char *filename);			//this writes a .raw file, can be opened with photoshop. Not req now using PNG
int bitmapDestroy(BM* bm);

int mixColours(COLOUR *cCanvas, COLOUR *cBrush);	//canvas gets written to

int DrawRegion(BM *bm, WORLDREGION *r);
int DrawListOfRegions(BM *bm, WORLDREGION *first);
int DrawGrid(BM* bm);
int ColourWheel(BM* bm, int x, int y, int r, int steps);
int PlotPaths(BM* bm, LOCATIONHISTORY *locationHistory, OPTIONS *options);

int CreateHeatmap(BM* bm);
int HeatmapAddPoint(HEATMAP *hm, int x, int y);
int HeatmapToBitmap(BM *bm);
int HeatmapPlot(BM* bm, LOCATIONHISTORY*lh);
unsigned char HeatmapIntToCharNormalisedLog(unsigned int Temp);
COLOUR HeatmapColour(unsigned char normalisedtemp);

COLOUR HsvToRgb(unsigned char h, unsigned char s,unsigned char v, unsigned char a);
//COLOUR TimestampToRgb(long ts, long min, long max);
COLOUR TimestampToRgb(long ts, long max);
COLOUR SpeedToRgb(double speed, double maxspeed);
COLOUR AccuracyToRgb(int accuracy);
COLOUR DayOfWeekToRgb(long ts, COLOUR *colourPerDayArrayOfSeven);	//needs to be an array of 7
COLOUR HourToRgb(long ts, COLOUR *cMidnight, COLOUR *cNoon);
COLOUR MonthToRgb(long ts, COLOUR *colourPerMonthArrayOfTwelve);

int LatLongToXY(BM *bm, double latitude, double longitude, double *x, double *y);	//lat, long, output point

int CoordInNSWE(WORLDCOORD *coord, NSWE *nswe);
int CopyNSWE(NSWE *dest, NSWE *src);
void IntersectionOfNSWEs(NSWE *output, NSWE *d1, NSWE *d2);
double AreaOfNSWE(NSWE *nswe);

double ipart(double x);
double round(double x);
double fpart(double x);
double rfpart(double x);
int plot(BM* bm, int x, int y, unsigned char cchar, COLOUR *c);

double MetersApartFlatEarth(double lat1, double long1, double lat2, double long2);	//takes degrees, this uses "Polar Coordinate Flat-Earth Formula"



WORLDREGION * CreateRegion(WORLDREGION * parentRegion, NSWE *nswe, COLOUR *c);
TRIP * GetLinkedListOfTrips(NSWE * home, NSWE * away, WORLDREGION * excludedRegions, LOCATIONHISTORY *lh);
int ExportTripData(TRIP * trip, char * filename);
int FreeLinkedListOfTrips(TRIP * trip);


//Graphs
void GraphScatter(BM *bm, COLOUR *cBackground, double minx, double miny, double maxx, double maxy, double xmajorunit, double ymajorunit,\
	 COLOUR *cAxisAndLabels, char * xaxislabel, char * yaxislabel, \
	 COLOUR *cDataColour, int markerwidth, int numberofpoints, double *xarray, double *yarray);
