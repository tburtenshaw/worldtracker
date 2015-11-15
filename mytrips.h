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

	double west;
	double east;
	double north;
	double south;

	double zoom;

	unsigned long	fromtimestamp;
	unsigned long	totimestamp;

	int thickness;		//the thickness of the line
	COLOUR gridcolour;
	int gridsize;
	long colourcycle;	//number of seconds before going red to red. Defaults to six months
};


struct sBitmap	{
	int width;
	int height;

	//double west; //move to options
	//double east;
	//double north;
	//double south;

	double zoom;	//on a full map, this is the number of pixels per degree

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
	//char *jsonfilename;

	unsigned long	numPoints;
	unsigned long	earliesttimestamp;
	unsigned long	latesttimestamp;
	LOCATION * first;
	LOCATION * last;
};


int LoadLocations(LOCATIONHISTORY *locationHistory, char *jsonfilename);
int FreeLocations(LOCATIONHISTORY *locationHistory);
int ReadLocation(LOCATIONHISTORY *lh, LOCATION *location);

int LoadPreset(OPTIONS *options, char *preset);
int RationaliseOptions(OPTIONS *options);
int MakeProperFilename(char *targetstring, char *source, char *def, char *ext);

int WriteKMLFile(BM* bm);

int bitmapInit(BM* bm, OPTIONS* options, LOCATIONHISTORY *lh);
int bitmapPixelSet(BM* bm, int x, int y, COLOUR c);
COLOUR bitmapPixelGet(BM* bm, int x, int y);
int bitmapFilledCircle(BM* bm, double x, double y, double radius, COLOUR c);
int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, int thickness, COLOUR c);
int bitmapCoordLine(BM *bm, double lat1, double lon1, double lat2, double lon2, int thickness, COLOUR c);

int bitmapWrite(BM* bm, char *filename);			//this writes a .raw file, can be opened with photoshop. Not req now using PNG
int bitmapDestroy(BM* bm);

int mixColours(COLOUR *cCanvas, COLOUR *cBrush);	//canvas gets written to

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
COLOUR TimestampToRgb(long ts, long min, long max);

int LatLongToXY(BM *bm, double latitude, double longitude, double *x, double *y);	//lat, long, output point

double ipart(double x);
double round(double x);
double fpart(double x);
double rfpart(double x);
int plot(BM* bm, int x, int y, unsigned char cchar, COLOUR c);

