/* This contains all the functions that are shared between the command line and Windows program */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>


typedef struct sRGBAColour COLOUR;
typedef struct sBitmap BM;
typedef struct sOptions OPTIONS;
typedef struct sLocationHistory LOCATIONHISTORY;
typedef struct sLocation LOCATION;
typedef struct sImportedFile IMPORTEDFILE;

typedef struct sNswe NSWE;
typedef struct sStay STAY;
typedef struct sTrip TRIP;
typedef struct sWorldCoord WORLDCOORD;
typedef struct sWorldRegion WORLDREGION;	//this can be a linked list
typedef struct sPreset PRESET;
typedef struct sRasterFont RASTERFONT;
typedef struct sRasterChar RASTERCHAR;

//#define FILETYPE_JSON 1
//#define FILETYPE_NMEA 2
//#define FILETYPE_GPX 3
//#define FILETYPE_BACKITUDECSV 3

typedef enum Input_Filetype
{
	FT_UNKNOWN,
	FT_JSON,
	FT_NMEA,
	FT_GPX,
	FT_BACKITUDECSV
} Input_Filetype;

#define MAX_DIMENSION 4096*2
#define PI 3.14159265
#define EARTH_MEAN_RADIUS_KM 6371

#define COLOUR_BY_TIME 0
#define COLOUR_BY_SPEED 1
#define COLOUR_BY_ACCURACY 2
#define COLOUR_BY_DAYOFWEEK 3
#define COLOUR_BY_HOUR 4
#define COLOUR_BY_MONTH 5
#define COLOUR_BY_GROUP 6
#define COLOUR_BY_FADEOVERTIME 7

#define MAX_COLOURBY_OPTION COLOUR_BY_FADEOVERTIME

#define REGIONTYPE_HOME 1
#define REGIONTYPE_AWAY 2
#define REGIONTYPE_EXCLUSION 3

struct sRGBAColour	{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
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
	unsigned long countPoints;	//the number of points plotted (statistics)
};


struct sLocation	{
	double latitude;
	double longitude;
	unsigned long timestampS; //we'll use a long instead of the high precision of google (seconds rather than ms)
	int accuracy;
	int altitude;
	int inputfileindex;
	int group;	//which input file it was loaded from

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

	IMPORTEDFILE * firstImportedFile;


	int numinputfiles;	//number of files used for input
	int numgroups;	//groups of input files, initially 1:1, but we can merge them

	unsigned long	numPoints;
	unsigned long	earliesttimestamp;
	unsigned long	latesttimestamp;
	NSWE bounds;
	LOCATION * first;
	LOCATION * last;
};

struct sImportedFile	{
	int id;
	int group;
	char fullFilename[MAX_PATH];
	char displayFilename[MAX_PATH];
	unsigned long	filesize;
	Input_Filetype filetype;

	IMPORTEDFILE * next;
};

struct sStay	{
	STAY *next;
	unsigned long	leavetime;
	unsigned long	arrivetime;
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
	char * title;
	int type;
	WORLDREGION * next;	//next in the linked list
	WORLDREGION * prev;	//next in the linked list
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
LOCATION *  DeleteLocation(LOCATIONHISTORY *lh, LOCATION *locToDelete);	//returns the "next" location

void RemoveLocationFromList(LOCATION **ppFirst, LOCATION **ppLast, LOCATION *loc);
void InsertLocationBefore(LOCATION **ppFirst, LOCATION *loc, LOCATION *target);
int SortLocationsInsertSort(LOCATION **ppFirst, LOCATION **ppLast);	//pointer to pointer of first and last - it edits the values
int MergeLocationGroup(LOCATION **ppFirstBase, LOCATION **ppLastBase, LOCATION **ppFirstNew, LOCATION **ppLastNew);
int OptimiseLocations(LOCATIONHISTORY *locationHistory);
int PrintLocations(LOCATIONHISTORY *locationHistory); //for debugging

int ReadLocation(LOCATIONHISTORY *lh, LOCATION *location, int filetype);
int ReadLocationFromJson(LOCATIONHISTORY *lh, LOCATION *location);
int ReadLocationFromNmea(LOCATIONHISTORY *lh, LOCATION *location);
int ReadLocationFromBackitudeCSV(LOCATIONHISTORY *lh, LOCATION *location);
int GuessInputFileType(LOCATIONHISTORY *lh);

int	AddInputFile(LOCATIONHISTORY *lh, IMPORTEDFILE *importedFile);
IMPORTEDFILE * GetInputFileByIndex(LOCATIONHISTORY *lh, int id);
int DeleteInputFile(LOCATIONHISTORY *lh, int id);

void LoadPresets(PRESET *preset, int * pCount, int maxCount);
int NsweFromPreset(OPTIONS *options, char *lookuppreset, PRESET * presetarray, int numberofpresets);
char * SuggestAreaFromNSWE(NSWE* viewport, PRESET * presetarray, int numberofpresets);
int GetBestPresets(char *searchterm, PRESET *presetlist, int countlist, PRESET *presetbest, int countbest);

int RationaliseOptions(OPTIONS *options);
int MakeProperFilename(char *targetstring, char *source, char *def, char *ext);

int WriteKMLFile(BM* bm);
int ExportGPXFile(LOCATIONHISTORY *lh, char * GPXFilename, unsigned long tsfrom, unsigned long tsto);

int bitmapInit(BM* bm, OPTIONS* options, LOCATIONHISTORY *lh);
int bitmapPixelSet(BM* bm, int x, int y, COLOUR *c);
COLOUR bitmapPixelGet(BM* bm, int x, int y);
int bitmapFilledCircle(BM* bm, int x, int y, int diameter, COLOUR *c);	//I'm now using the top left coords and a width
int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, int thickness, COLOUR *c);
int bitmapCoordLine(BM *bm, double lat1, double lon1, double lat2, double lon2, int thickness, COLOUR *c);
int bitmapSquare(BM* bm, int x0, int y0, int x1, int y1, COLOUR *cBorder, COLOUR *cFill);
int bitmapText(BM * bm, RASTERFONT *rasterfont, int x, int y, char * szText, COLOUR * colourText);

void plotOctet(BM* bm, int cx, int cy, int col, int row, int isoddwidth, int alpha, COLOUR *c);	//plots 8 points (mirroring the octant)
int plot(BM* bm, int x, int y, unsigned char cchar, COLOUR *c);

int bitmapWrite(BM* bm, char *filename);			//this writes a .raw file, can be opened with photoshop. Not req now using PNG
int bitmapDestroy(BM* bm);

int mixColours(COLOUR *cCanvas, COLOUR *cBrush);	//canvas gets written to

int DrawRegion(BM *bm, WORLDREGION *r);
int DrawListOfRegions(BM *bm, WORLDREGION *first);
int DrawGrid(BM* bm);
int ColourWheel(BM* bm, int x, int y, int r, int steps);
int PlotPaths(BM* bm, LOCATIONHISTORY *locationHistory, OPTIONS *options);


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

double MetersApartFlatEarth(double lat1, double long1, double lat2, double long2);	//takes degrees, this uses "Polar Coordinate Flat-Earth Formula"
double MetersApartHaversine(double lat1, double long1, double lat2, double long2);


int GetPresetScoreArray(char *searchtext, PRESET *presetlist, int countlist, int *scorearray);

int DateRangeInNSWE(LOCATIONHISTORY *locationHistory, NSWE *viewport, unsigned long * from, unsigned long * to);

WORLDREGION * CreateRegionAfter(WORLDREGION * parentRegion, NSWE *nswe, char * title, int type, COLOUR *c, WORLDREGION ** pRegionHead);
int DeleteRegion(WORLDREGION * regionToDelete, WORLDREGION ** pRegionHead);
int DeleteRegionByIndex(WORLDREGION ** pRegionHead, int index);

TRIP * GetLinkedListOfTrips(WORLDREGION * regions, LOCATIONHISTORY *lh);
int ExportTripData(TRIP * trip, char * filename);
int FreeLinkedListOfTrips(TRIP * trip);

STAY * CreateStayListFromNSWE(NSWE * nswe, LOCATIONHISTORY *lh);
long SecondsInStay(STAY *stay, long starttime, long endtime);
int ExportTimeInNSWE(NSWE *nswe, long starttime, long endtime, long interval, LOCATIONHISTORY *lh);
int FreeLinkedListOfStays(STAY * stay);

void HeatMap(BM *bm, LOCATIONHISTORY *lh, OPTIONS *options, int blocksX, int blocksY);
void DrawBackground(BM *bm);

//Graphs
void GraphScatter(BM *bm, COLOUR *cBackground, double minx, double miny, double maxx, double maxy, double xmajorunit, double ymajorunit,\
	 COLOUR *cAxisAndLabels, char * xaxislabel, char * yaxislabel, void(*xlabelfn)(double, char *), void(*ylabelfn)(double, char *),\
	 COLOUR *cDataColour, int markerwidth, int numberofpoints, double *xarray, double *yarray);

void GraphLine(BM *bm, COLOUR *cBackground, double minx, double miny, double maxx, double maxy, COLOUR *cDataColour, int numberofpoints, double *xarray, double *yarray);


void labelfnShortDayOfWeekFromSeconds(double seconds, char * outputString);
void labelfnTimeOfDayFromSeconds(double seconds, char * outputString);
void labelfnMinutesFromSeconds(double seconds, char * outputString);

