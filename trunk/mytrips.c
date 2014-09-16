//current version
#define VERSION 0.28
#define MAX_DIMENSION 4096*2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "lodepng.h"

#define PI 3.14159265

typedef struct sRGBAColour COLOUR;
typedef struct sBitmap BM;
typedef struct sOptions OPTIONS;
typedef struct sLocation LOCATION;
typedef struct sLocationHistory LOCATIONHISTORY;

struct sRGBAColour	{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};

struct sOptions	{	//what we can get from the command line
	char *jsonfilename;	//these are just pointers to either the default, or the command line argument
	char *pngfilename;
	char *kmlfilename;

	int width;
	int height;

	double west;
	double east;
	double north;
	double south;

	double zoom;

	unsigned long	fromtimestamp;
	unsigned long	totimestamp;

	int gridsize;
	long colourcycle;	//number of seconds before going red to red. Defaults to six months
};


struct sBitmap	{
	char pngfilename[256];	//this actually holds the filename we'll use
	char kmlfilename[256];	//this actually holds the filename we'll use
	char jsonfilename[256];	//this actually holds the filename we'll use

	int width;
	int height;

	double west;
	double east;
	double north;
	double south;

	double zoom;	//on a full map, this is the number of pixels per degree

	char *bitmap;	//always going to be a four channel RGBA bitmap now
	int sizebitmap;

	LOCATIONHISTORY *lh;	//pointer to the location history associated with this bitmap
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
int MakeProperFilename(char *targetstring, char *source, char *def, char *ext);

int WriteKMLFile(BM* bm);

int bitmapInit(BM* bm, int width, int height, double zoom, double north, double south, double west, double east);
int bitmapPixelSet(BM* bm, int x, int y, COLOUR c);
int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, COLOUR c);
int bitmapCoordLine(BM *bm, double lat1, double lon1, double lat2, double lon2, COLOUR c);

int bitmapWrite(BM* bm, char *filename);			//this writes a .raw file, can be opened with photoshop. Not req now using PNG
int bitmapDestroy(BM* bm);

int mixColours(COLOUR *cCanvas, COLOUR *cBrush);	//canvas gets written to

int DrawGrid(BM* bm, int spacing, COLOUR c);
int ColourWheel(BM* bm, int x, int y, int r, int steps);


COLOUR HsvToRgb(unsigned char h, unsigned char s,unsigned char v, unsigned char a);
COLOUR TimestampToRgb(long ts, long min, long max);

int LatLongToXY(BM *bm, double phi, double lambda, double *x, double *y);	//lat, long, output point

double ipart(double x);
double round(double x);
double fpart(double x);
double rfpart(double x);
int plot(BM* bm, int x, int y, double cdouble, COLOUR c);

void PrintIntro(char *programName);	//blurb
void PrintUsage(char *programName); 	//called if run without arguments
//int HandleOptions(int argc,char *argv[]);

void PrintIntro(char *programName)
{
	fprintf(stdout, "World Tracker  - Version %2.2f\r\n", VERSION);
	fprintf(stdout, "Plots out your Google Location history\r\n\r\n");
	fprintf(stdout, "Copyright © 2014 Tristan Burtenshaw\r\n");
	fprintf(stdout, "New BSD Licence (\'%s --copyright' for more information.)\r\n", programName);
	fprintf(stdout, "Contains LodePNG library by Lode Vandevenne (http://lodev.org/lodepng/)\r\n\r\n");
}

void PrintUsage(char *programName)
{
	fprintf(stdout, "Usage: %s [-n <north>] [-s <south>] [-w <west>] [-e <east>] [-x <width>] [-y <height>] [<other options>] [-i <input.json>] [-k <output.kml>] [[-o] <output.png>]\r\n\r\n", programName);
	fprintf(stdout, "Default input: \'LocationHistory.json\' from the current folder.\r\n");
	fprintf(stdout, "Default output: \'trips.png\'.\r\n");
}

int HandleOptions(int argc,char *argv[], OPTIONS *options)
{
	int i,firstnonoption=0;

	for (i=1; i< argc;i++) {
		if (argv[i][0] == '/' || argv[i][0] == '-') {
			switch (argv[i][1]) {
				/* An argument -? means help is requested */
				case '?':
					PrintUsage(argv[0]);
					break;
				case 'h':
				case 'H':
					if (!stricmp(argv[i]+1,"help")) {
						PrintUsage(argv[0]);
					}
					if (!stricmp(argv[i]+1,"height")) {	//also might mean height
						if (i+1<argc)	{
							options->height = strtol(argv[i+1], NULL, 0);
							i++;
						}
					}
					break;
				case 'n':
				case 'N':
					if (!stricmp(argv[i]+1,"north") || *(argv[i]+2)==0)	{	//if it's north or n (terminated with 0)
						if (i+1<argc)	{
							options->north = strtod(argv[i+1], NULL);
							i++;
						}
					}
					break;
				case 's':
				case 'S':
					if (!stricmp(argv[i]+1,"south") || *(argv[i]+2)==0)	{	//if it's north or n (terminated with 0)
						if (i+1<argc)	{
							options->south = strtod(argv[i+1], NULL);
							i++;
						}
					}
					break;
				case 'w':
				case 'W':
					if (!stricmp(argv[i]+1,"west") || *(argv[i]+2)==0)	{	//if it's north or n (terminated with 0)
						if (i+1<argc)	{
							options->west = strtod(argv[i+1], NULL);
							i++;
						}
					}
					if (!stricmp(argv[i]+1,"width")) {	//also might mean height
						if (i+1<argc)	{
							options->width = strtol(argv[i+1], NULL, 0);
							i++;
						}

					}

					break;
				case 'e':
				case 'E':
					if (!stricmp(argv[i]+1,"east") || *(argv[i]+2)==0)	{	//if it's north or n (terminated with 0)
						if (i+1<argc)	{
							options->east = strtod(argv[i+1], NULL);
							i++;
						}
					}
					break;
				case 'x':
				case 'X':
					if (i+1<argc)	{
						options->width = strtol(argv[i+1], NULL, 0);
						i++;
					}
					break;
				case 'y':
				case 'Y':
					if (i+1<argc)	{
						options->height = strtol(argv[i+1], NULL, 0);
						i++;
					}
					break;


				case 'z':
				case 'Z':
					if (i+1<argc)	{
						options->zoom = strtod(argv[i+1], NULL);
						i++;	//move to the next variable, we've got a zoom
					}
					break;
				case 'g':
				case 'G':
					if (i+1<argc)	{
						options->gridsize = strtod(argv[i+1], NULL);
						i++;
					}
					break;
				case 'c':
				case 'C':
					if (i+1<argc)	{
						if (!stricmp(argv[i+1],"day"))	options->colourcycle=60*60*24;
						else	if (!stricmp(argv[i+1],"week"))	options->colourcycle=60*60*24*7;
						else	if (!stricmp(argv[i+1],"month"))	options->colourcycle=60*60*24*30.4375;
						else	if (!stricmp(argv[i+1],"halfyear"))	options->colourcycle=60*60*24*182.625;
						else	if (!stricmp(argv[i+1],"year"))	options->colourcycle=60*60*24*365.25;
						else	if (!stricmp(argv[i+1],"hour"))	options->colourcycle=60*60;
						else	options->colourcycle = strtol(argv[i+1], NULL, 0);

						i++;
					}
					break;

				case 'p':
				case 'P':
					if (!stricmp(argv[i]+1,"preset") || *(argv[i]+2)==0)	{	//preset or just p
						if (i+1<argc)	{
							LoadPreset(options, argv[i+1]);
							i++;
						}

					}
					break;
				case 'f':	//from
				case 'F':
					if (i+1<argc)	{
						options->fromtimestamp = strtol(argv[i+1], NULL,0);
						i++;
					}
					break;
				case 't':	//to
				case 'T':
					if (i+1<argc)	{
						options->totimestamp = strtol(argv[i+1], NULL,0);
						i++;
					}
					break;
				case 'k':
				case 'K':
					if (i+1<argc)	{
						options->kmlfilename = argv[i+1];
						i++;
					}
					break;
				case 'i':
				case 'I':
					if (i+1<argc)	{
						options->jsonfilename = argv[i+1];
						i++;
					}
					break;
				case 'o':
				case 'O':
					if (i+1<argc)	{
						options->pngfilename = argv[i+1];
						i++;
					}
					break;
				default:
					fprintf(stderr,"unknown option %s\n",argv[i]);
					break;
			}
		}
		else {
			options->pngfilename = argv[i];
			firstnonoption = i;
			break;
		}
	}
	return firstnonoption;
}

int main(int argc,char *argv[])
{
	BM	mainBM;
	OPTIONS options;

	double west;
	double east;
	double north;
	double south;
	double tempdouble;

	int height, width;
	int testwidth;
	//int testheight;
	double zoom;
	COLOUR c;

	LOCATIONHISTORY locationHistory;

	unsigned long countPoints;
	double oldlat, oldlon;

	int error;

	LOCATION *coord;

	//set vars to zero
	memset(&options, 0, sizeof(options));	//set all the option results to 0
	memset(&mainBM, 0, sizeof(mainBM));
	mainBM.lh=&locationHistory;


	//Display introductory text
	PrintIntro(argv[0]);

	//locationHistory.jsonfilename="LocationHistory.json";	//default

	if (argc == 1)	PrintUsage(argv[0]);	//print usage instructions
	HandleOptions(argc, argv, &options);

	zoom=options.zoom;
	north=options.north;
	south=options.south;
	west=options.west;
	east=options.east;

	width=options.width;
	height=options.height;

	//print some of the options
	fprintf(stdout, "From the options:\r\n");
	fprintf(stdout, "Input file: %s\r\n", options.jsonfilename);
	fprintf(stdout, "N %f, S %f, W %f, E %f\r\n",north,south,west,east);
	fprintf(stdout, "KML: %s\r\n",options.kmlfilename);
	fprintf(stdout, "\r\n");

	if (options.colourcycle==0) options.colourcycle = (60*60*24*183);

	//Load appropriate filenames
	MakeProperFilename(mainBM.pngfilename, options.pngfilename, "trips.png", "png");
		char pngnoext[256];
		char *period;


	if (options.kmlfilename==NULL)	{
		strcpy(&pngnoext[0], mainBM.pngfilename);	//copy the png filename
		period=strrchr(pngnoext,'.');			//find the final period
		*period=0;								//replace it with a null to end it
		fprintf(stdout, "KML no ext: %s\r\n",pngnoext);
		MakeProperFilename(mainBM.kmlfilename, pngnoext, "fallback.kml", "kml");
	} else
	{
		MakeProperFilename(mainBM.kmlfilename, options.kmlfilename, "fallback.kml", "kml");
	}

	MakeProperFilename(mainBM.jsonfilename, options.jsonfilename, "LocationHistory.json", "json");
	fprintf(stdout, "KML: %s\r\n",mainBM.kmlfilename);

	//Set the zoom
	if ((zoom<0.1) || (zoom>55))	{	//have to decide the limit, i've tested to 55
		if (zoom) fprintf(stderr, "Zoom must be between 0.1 and 55\r\n");	//if they've entered something silly
		zoom = 0;	//set to zero, then we'll calculate.
	}

	//if they're the wrong way around
	if (east<west)	{tempdouble=east;east=west;west=tempdouble;}
	if (north<south)	{tempdouble=north;north=south;south=tempdouble;}

	//if they're strange
	if ((east-west == 0) || (north-south==0) || (east-west>360) || (north-south>360))	{
		west=-180;
		east=180;
		north=90;
		south=-90;
	}


	if ((height==0) && (width == 0))	{	//if no height or width specified, we'll base it on zoom (or default zoom)
		if (zoom==0)	{zoom=10;}	//default zoom
		width=360*zoom;
		height= 180*zoom;
	}	else
	if (width==0)	{	//the haven't specified a width (but have a height)
		width=height*(east-west)/(north-south);
		if (width > MAX_DIMENSION)	{	//if we're oversizing it
			width = MAX_DIMENSION;
			height = width*(north-south)/(east-west);
		}
	}	else
	if (height==0)	{	//the haven't specified a height (but have a width)
			height=width*(north-south)/(east-west);
			if (height > MAX_DIMENSION)	{	//if we're oversizing it
				height = MAX_DIMENSION;
				width=height*(east-west)/(north-south);
			}
	}


	//test for strange rounding errors
	if ((width==0) || (height==0) || (height > MAX_DIMENSION) || (width > MAX_DIMENSION))	{
		fprintf(stderr, "Problem with dimensions (%i x %i). Loading small default size.\r\n", width, height);
		west=-180;
		east=180;
		north=90;
		south=-90;
		height=180;
		width=360;
	}

	//if the aspect ratio of coords is different, set the width to be related to the
	testwidth=height*(east-west)/(north-south);
	if (testwidth != width)	{
		printf("Fixing aspect ratio. tw: %i, w: %i\r\n", testwidth, width);
		width=testwidth;
	}

	//then calculate how many pixels per degree
	zoom=width/(east-west);
	fprintf(stdout, "Zoom: %4.2f\r\n", zoom);

	//Set the from and to times
	if (options.totimestamp == 0)
		options.totimestamp =-1;


	//Initialise the bitmap
	bitmapInit(&mainBM, width, height, zoom, north, south, west, east);

	//Draw grid
	c.R=192;c.G=192;c.B=192;c.A=128;
	DrawGrid(&mainBM, options.gridsize, c);
	//ColourWheel(mainBM, 100, 100, 100, 5);  	//Color wheel test of lines and antialiasing

	oldlat=0;oldlon=0;
	locationHistory.earliesttimestamp=-1;
	locationHistory.latesttimestamp=0;
	countPoints=0;

	fprintf(stdout, "Loading locations from %s.\r\n", mainBM.jsonfilename);
	LoadLocations(&locationHistory, &mainBM.jsonfilename[0]);

	fprintf(stdout, "Plotting paths.\r\n");
	coord=locationHistory.first;
	while (coord)	{

		c=TimestampToRgb(coord->timestampS, 0, options.colourcycle);	//about 6 month cycle
		//c=TimestampToRgb(coord->timestampS, 1300000000, 1300000000+(60*60*24));	//day cycle
		if (coord->accuracy <200000 && coord->timestampS >= options.fromtimestamp && coord->timestampS <= options.totimestamp)	{
			//draw a line from last point to the current one.
			if (countPoints>0)	{
				bitmapCoordLine(&mainBM, coord->latitude, coord->longitude, oldlat, oldlon, c);
			}

			oldlat=coord->latitude;oldlon=coord->longitude;
			countPoints++;
		}

		//i'll move this outside the loop for speed reasons
		if (zoom>1000)	coord=coord->next;
		else if (zoom>100) coord=coord->next1000ppd;
		else if (zoom>10) coord=coord->next100ppd;
		else if (zoom>1) coord=coord->next10ppd;
		else coord=coord->next1ppd;
	}

	fprintf(stdout, "Ploted: %i points\r\n", countPoints);
//	fprintf(stdout, "Earliest: %i\tLastest: %i\r\n", locationHistory.earliesttimestamp, locationHistory.latesttimestamp);
	printf("From:\t%s\r\n", asctime(localtime(&locationHistory.earliesttimestamp)));
	printf("To:\t%s\r\n", asctime(localtime(&locationHistory.latesttimestamp)));

	FreeLocations(&locationHistory);


	//Write the PNG file
	fprintf(stdout, "Writing to %s.\r\n", mainBM.pngfilename);
	error = lodepng_encode32_file(mainBM.pngfilename, mainBM.bitmap, mainBM.width, mainBM.height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));

	//Write KML file
	fprintf(stdout, "Writing to %s.\r\n", mainBM.kmlfilename);
	WriteKMLFile(&mainBM);

	bitmapDestroy(&mainBM);
	fprintf(stdout, "Program finished.");
	return 0;
}


int LoadLocations(LOCATIONHISTORY *locationHistory, char *jsonfilename)
{
	LOCATION *coord;
	LOCATION *prevCoord;

	LOCATION *waitingFor1;
	LOCATION *waitingFor10;
	LOCATION *waitingFor100;
	LOCATION *waitingFor1000;


	//Open the input file
	locationHistory->json=fopen(jsonfilename,"r");
	if (locationHistory->json==NULL)	{
		fprintf(stderr, "\r\nUnable to open \'%s\'.\r\n", jsonfilename);
		perror("Error");
		fprintf(stderr, "\r\n");
		return 1;
	}


	//Now read it and load it into a linked list
	prevCoord=NULL;
	coord=malloc(sizeof(LOCATION));
	locationHistory->first = coord;
	waitingFor1 = coord;
	waitingFor10 = coord;
	waitingFor100 = coord;
	waitingFor1000 = coord;

	while (ReadLocation(locationHistory, coord)==1)	{
		//get the timestamp max and min
		if (coord->timestampS > locationHistory->latesttimestamp)
			locationHistory->latesttimestamp = coord->timestampS;
		if (coord->timestampS<locationHistory->earliesttimestamp)
			locationHistory->earliesttimestamp = coord->timestampS;
		locationHistory->numPoints++;

		//set the next of the previous depending on our zoom resolution
		if ((fabs(waitingFor1->latitude - coord->latitude) >1) ||(fabs(waitingFor1->latitude - coord->latitude) >1))	{
			waitingFor1->next1ppd = coord;
			waitingFor1=coord;
		}

		if ((fabs(waitingFor10->latitude - coord->latitude) >0.1) ||(fabs(waitingFor10->latitude - coord->latitude) >0.1))	{
			waitingFor10->next10ppd = coord;
			waitingFor10=coord;
		}

		if ((fabs(waitingFor100->latitude - coord->latitude) >0.01) ||(fabs(waitingFor100->latitude - coord->latitude) >0.01))	{
			waitingFor100->next100ppd = coord;
			waitingFor100=coord;
		}

		if ((fabs(waitingFor1000->latitude - coord->latitude) >0.001) ||(fabs(waitingFor1000->latitude - coord->latitude) >0.001))	{
			waitingFor1000->next1000ppd = coord;
			waitingFor1000=coord;
		}


		coord->prev=prevCoord;
		coord->next=malloc(sizeof(LOCATION));	//allocation memory for the next in the linked list
		prevCoord=coord;

		//Advance to the next in the list
		coord= coord->next;
		//printf("%i/t",(long)coord);
	}
	free(coord);//remove the last allocated
	if (prevCoord)	{
		prevCoord->next=NULL;	//close off the linked list
		locationHistory->last=prevCoord;
	}

	fclose(locationHistory->json);
	return 0;
}

int FreeLocations(LOCATIONHISTORY *locationHistory)
{
	LOCATION *loc;
	LOCATION *locprev;

	loc = locationHistory->last;

	while (loc)	{
		locprev=loc->prev;
		free(loc);
		loc=locprev;
	}

	return 0;
}

int ReadLocation(LOCATIONHISTORY *lh, LOCATION *location)
{
	int step;
	char buffer[256];
	char *x;
	char *y;

	step=0;
	while (fgets(buffer, 256, lh->json) != NULL)	{
		if (step==0)	{
			x=strstr(buffer, "timestampMs");
			if (x)	{
				x=strchr(x,':');
				//printf("ts %s\r\n",x);
				if (x)	x=strchr(x,'\"')+1;
				if (x)  y=strchr(x,'\"')-3;
				y[0]=0;
				location->timestampS=strtol(x, NULL, 10);
				step++;
				//printf("%i\r\n",location->timestampS);
			}
		}

		if (step==1)	{
			x=strstr(buffer, "latitudeE7");
			if (x)	{
				x=strchr(x,':')+1;

				location->latitude=strtod(x, NULL)/10000000;
				step++;
			}
		}
		if (step==2)	{
			x=strstr(buffer, "longitudeE7");
			if (x)	{
				x=strchr(x,':')+1;
				location->longitude=strtod(x, NULL)/10000000;
				step++;
			}
		}

		if (step==3)	{
			x=strstr(buffer, "accuracy");
			if (x)	{
				x=strchr(x,':')+1;
				location->accuracy=strtol(x, NULL, 10);
				return 1;
			}
		}



	}
	fprintf (stdout, "Finished reading JSON file.\r\n");
	return 0;
};


int bitmapInit(BM *bm, int xsize, int ysize, double zoom, double north, double south, double west, double east)
{
	char* bitmap;

	bm->width=xsize;
	bm->height=ysize;
	bm->sizebitmap=xsize*ysize*4;
	bm->zoom=zoom;
	bm->north=north;
	bm->south=south;
	bm->west=west;
	bm->east=east;

	bitmap=(char*)calloc(bm->sizebitmap, sizeof(char));
	//memset(bitmap, 0, bm->sizebitmap);

	printf("New bitmap width: %i, height: %i\r\n", xsize, ysize);

	bm->bitmap=bitmap;

	return 1;
}

int bitmapDestroy(BM *bm)
{
	free(bm->bitmap);
	return 0;
}

int bitmapPixelSet(BM* bm, int x, int y, COLOUR c)
{
	COLOUR currentC;

	if (x>bm->width-1)	return 0;
	if (y>bm->height-1)	return 0;
	if (x<0)	return 0;
	if (y<0)	return 0;

	currentC.R = bm->bitmap[(x+y* bm->width) *4];
	currentC.G = bm->bitmap[(x+y* bm->width) *4+1];
	currentC.B = bm->bitmap[(x+y* bm->width) *4+2];
	currentC.A = bm->bitmap[(x+y* bm->width) *4+3];

	if (mixColours(&currentC, &c))	{
			memset(bm->bitmap + (x + y * bm->width) * 4+0,currentC.R,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+1,currentC.G,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+2,currentC.B,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+3,currentC.A,1);	//the *4 is the channels
	}



	return 1;
}

int mixColours(COLOUR *cCanvas, COLOUR *cBrush)
{

	int r,g,b,a;

	//first work out the alpha
	a=cCanvas->A+cBrush->A;
	if (a>255)
			a = 255;

	if (a>0)	{	//only bother writing if there is an alpha

		//printf("r%i g%i b%i a%i + r%i g%i b%i a%i = ",cCanvas->R,cCanvas->G,cCanvas->B,cCanvas->A, cBrush->R,cBrush->G,cBrush->B,cBrush->A);

		r = (cCanvas->R*(255-cBrush->A)+cBrush->R*cBrush->A);	//mix proportionally to the max alpha
		g = (cCanvas->G*(255-cBrush->A)+cBrush->G*cBrush->A);
		b = (cCanvas->B*(255-cBrush->A)+cBrush->B*cBrush->A);
		r/=a;
		g/=a;
		b/=a;

		if (r>255)	r=255;
		if (g>255)	g=255;
		if (b>255)	b=255;
		//printf("r%i g%i b%i a%i\r\n",r,g,b,a);

	}
		else
	return 0;

	//set the canvas
	cCanvas->R=r;
	cCanvas->G=g;
	cCanvas->B=b;
	cCanvas->A=a;
	return 1;
}

int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, COLOUR c)
{
	int steep;
	double tempdouble;

	double dx,dy;
	double gradient;
	double xend,yend;
	double intery;
	double xgap;
	double xpxl1, ypxl1;
	double xpxl2, ypxl2;

	double x;

	//Convert to int for now, until we get the errors sorted
	x0=(int)x0;
	y0=(int)y0;
	x1=(int)x1;
	y1=(int)y1;


	//based on the wikipedia article
	//printf("Line: %f,%f to %f,%f [R%iG%iB%i]", x0,y0,x1,y1, c.R, c.G, c.B);
	steep=(abs(y1 - y0) > abs(x1 - x0));

	if (steep)	{
		tempdouble = x0; x0=y0; y0=tempdouble;
		tempdouble = x1; x1=y1; y1=tempdouble;
	}

	if (x0>x1)	{
		tempdouble = x1; x1=x0; x0=tempdouble;	//swap x0,x1
		tempdouble = y1; y1=y0; y0=tempdouble;  //swap y0,y1
	}

	dx=x1-x0;
	dy=y1-y0;

	gradient = dy/dx;

	     // handle first endpoint
     xend = round(x0);
     yend = y0 + gradient * (xend - x0);
     xgap = rfpart(x0 + 0.5);
     xpxl1 = xend;   //this will be used in the main loop
     ypxl1 = ipart(yend);
     if (steep)	{

		 plot(bm, ypxl1,   xpxl1, rfpart(yend) * xgap, c);
		 plot(bm, ypxl1+1, xpxl1,  fpart(yend) * xgap, c);
		}
     else	{
         plot(bm, xpxl1, ypxl1  , rfpart(yend) * xgap, c);
         plot(bm, xpxl1, ypxl1+1,  fpart(yend) * xgap, c);
		}

     intery = yend + gradient; // first y-intersection for the main loop


	     // handle second endpoint
	 xend = round(x1);
     yend = y1 + gradient * (xend - x1);
     xgap = fpart(x1 + 0.5);
     xpxl2 = xend; //this will be used in the main loop
     ypxl2 = ipart(yend);
     if (steep)	{
         plot(bm, ypxl2  , xpxl2, rfpart(yend) * xgap, c);
         plot(bm, ypxl2+1, xpxl2,  fpart(yend) * xgap, c);

		}
     else	{
         plot(bm, xpxl2, ypxl2,  rfpart(yend) * xgap, c);
         plot(bm, xpxl2, ypxl2+1, fpart(yend) * xgap, c);

		}


     // main loop
	 for (x = xpxl1 + 1;x<xpxl2;x++)	{
         if  (steep)	{
             plot(bm, ipart(intery)  , x, rfpart(intery), c);
             plot(bm, ipart(intery)+1, x,  fpart(intery), c);
			 //printf("steep\r\n");
			}
         else	{
             plot(bm, x, ipart (intery),  rfpart(intery), c);
             plot(bm, x, ipart (intery)+1, fpart(intery), c);
//			 printf("\r\n");
			}
//			printf("i %f  f %f\r\n",ipart(intery), fpart(intery));
         intery = intery + gradient;
	}

	return 0;
}

int bitmapCoordLine(BM *bm, double lat1, double lon1, double lat2, double lon2, COLOUR c)
{
	double tempdouble;
	double dx,dy;
	double yintersect;
	double m;

	double x1,y1;
	double x2,y2;
	int swappedflag;


	//if the line is obviously out of the bounds of the bitmap then can return
	if ((lon1 < bm->west) && (lon2 < bm->west))	return 1;
	if ((lon1 > bm->east) && (lon2 > bm->east))	return 1;
	if ((lat1 < bm->south) && (lat2 < bm->south))	return 1;
	if ((lat1 > bm->north) && (lat2 > bm->north))	return 1;


	LatLongToXY(bm, lat1, lon1, &x1,&y1);
	LatLongToXY(bm, lat2, lon2, &x2,&y2);
	dx=x1-x2;
	dy=y1-y2;
	if ((abs(dx)>180*bm->zoom))	{	//they've moved over half the map
		swappedflag=0;	//flag
		if (dx >0)	{	//if it's the eastward direction then we'll swap vars
			//printf("Pretransform %i %i to %i %i, diff:%i %i c: %i %i\r\n", x2, y2, xi,yi,dx,dy, 0,yintersect);
			tempdouble=x1;x1=x2;x2=tempdouble;	//swap x
			tempdouble=y1;y1=y2;y2=tempdouble;	//swap y

			dx=x1-x2;	dy=y1-y2;
			swappedflag=1;	//flag to ensure we swap coords back
		}

		dx=x1-(x2-360*bm->zoom);	//x2-360*zoom is our new origin
		m=dy;	m/=dx;				//gradient, done this ugly way as doing division on ints

		yintersect = y2 + (360*bm->zoom-x2)*m;
//				printf("from %i %i to %i %i, diff:%i %i yint: %i m:%f\r\n", x2, y2, xi,yi,dx,dy, yintersect,m);

		//We draw two separate lines
		bitmapLineDrawWu(bm, x2,y2,360*bm->zoom-1, yintersect, c);
		bitmapLineDrawWu(bm, 0, yintersect, x1,y1, c);

		if (swappedflag==1)	{	//swap this back if we swapped!
			tempdouble=x1;x1=x2;x2=tempdouble;
			tempdouble=y1;y1=y2;y2=tempdouble;
		}

	}
	//Here is just the normal line from oldpoint to new one
	else	{
		bitmapLineDrawWu(bm, x2,y2,x1,y1,c);		//the normal case
	}


	return 1;
}

int bitmapWrite(BM *bm, char *filename)
{
	FILE* outputfile;
	int i;

	outputfile=fopen(filename, "wb");

	for (i=0;i<bm->sizebitmap;i++)	{
		fputc(bm->bitmap[i], outputfile);
	}


	fclose(outputfile);


	return 0;
}


//for the Wu algorithm in Wikipedia
double ipart(double x)
{
	int xi;

	xi=x;
	//printf("floor %f %f \r\n",x,floor(x));
	return (double)xi;
	//return floor(x);
}
double round(double x)
{
	return ipart(x+0.5);
}
double fpart(double x)
{
	return (x-ipart(x));
}
double rfpart(double x)
{
	return 1 - fpart(x);
}


int plot(BM* bm, int x, int y, double doublec, COLOUR c)
{

		 	c.A=(doublec)*(double)c.A;
			//printf("c.A %f\t %i\r\n",doublec, c.A);
		 	bitmapPixelSet(bm, x, y, c);

//printf("x %i y%i c%f \r\n",x,y,doublec);


return 0;
}

int LatLongToXY(BM *bm, double phi, double lambda, double *x, double *y)
{
	double width, height;
	width = bm->width;
	height = bm->height;

	*y=(bm->north - phi)/(bm->north-bm->south)*height;
	*x=(lambda - bm->west)/(bm->east-bm->west)*width;

	return 0;
}



COLOUR HsvToRgb(unsigned char h, unsigned char s,unsigned char v, unsigned char a)
{
    COLOUR rgb;
    unsigned char region, remainder, p, q, t;

    if (s == 0)
    {
        rgb.R = v;
        rgb.G = v;
        rgb.B = v;
        return rgb;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

	rgb.A=a;

    switch (region)
    {
        case 0:
            rgb.R = v; rgb.G = t; rgb.B = p;
            break;
        case 1:
            rgb.R = q; rgb.G = v; rgb.B = p;
            break;
        case 2:
            rgb.R = p; rgb.G = v; rgb.B = t;
            break;
        case 3:
            rgb.R = p; rgb.G = q; rgb.B = v;
            break;
        case 4:
            rgb.R = t; rgb.G = p; rgb.B = v;
            break;
        default:
            rgb.R = v; rgb.G = p; rgb.B = q;
            break;
    }

    return rgb;
}



COLOUR TimestampToRgb(long ts, long min, long max)
{
	double hue;
	long diff;

	diff=max-min;
	if (diff)	{		//avoid dividing by zero
		hue=(ts-min);
		hue=hue/diff;
		hue=hue*255;
	}
	else
		hue=0;

//	printf("%i\t%i\t%i\t%f\r\n", ts, diff, ts-min, hue);
	return HsvToRgb((char)hue,255,255,255);
}


int DrawGrid(BM* bm, int spacing, COLOUR c)
{
	//int i;
	int lat, lon;
	double x1,y1;
	double x2,y2;

	if ((spacing==0) || (spacing>180))
		return 0;

	for (lat=-90+spacing; lat<90; lat+=spacing)	{
		LatLongToXY(bm, lat, -180, &x1, &y1);
		LatLongToXY(bm, lat, 180, &x2, &y2);
//		printf("%f %f; %f %f\r\n",x1,y1,x2,y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2, c);
	}

	for (lon=-180+spacing; lon<180; lon+=spacing)	{
		LatLongToXY(bm, -90, lon, &x1, &y1);
		LatLongToXY(bm, 90, lon, &x2, &y2);
//		printf("%f %f; %f %f\r\n",x1,y1,x2,y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2, c);
	}


		//Equator
		lat=0;
		LatLongToXY(bm, lat, -180, &x1, &y1);
		LatLongToXY(bm, lat, 180, &x2, &y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2, c);
		c.A=c.A*0.8;
		bitmapLineDrawWu(bm, x1,y1-1,x2,y2-1, c);
		bitmapLineDrawWu(bm, x1,y1+1,x2,y2+1, c);

	return 0;
}


int ColourWheel(BM* bm, int x, int y, int r, int steps)
{
	int i;
	COLOUR c;
	if (steps<1) return 1;
 	for (i=0;i<360;i+=steps) {
 		c=HsvToRgb(i*255/360,255,255,255);
	 	bitmapLineDrawWu(bm, x,y,x+r*sin(i*PI/180),y+r*cos(i*PI/180),c);
 	}

	return 0;
}


int WriteKMLFile(BM* bm)
{
	FILE *kml;
	char sStartTime[80];
	char sEndTime[80];

	strftime (sStartTime,80,"%B %Y",localtime(&bm->lh->earliesttimestamp));
	strftime (sEndTime,80,"%B %Y",localtime(&bm->lh->latesttimestamp));

	kml=fopen(bm->kmlfilename,"w");
	fprintf(kml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	fprintf(kml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\r\n");
	fprintf(kml, "<GroundOverlay>\r\n");
	fprintf(kml, "<name>Your journey</name>\r\n");
	fprintf(kml, "<description>From %s to %s</description>\r\n", sStartTime, sEndTime);
	fprintf(kml, "<Icon><href>%s</href></Icon>\r\n",bm->pngfilename);
    fprintf(kml, "<LatLonBox>\r\n");
	fprintf(kml, "<north>%f</north>\r\n",bm->north);
	fprintf(kml, "<south>%f</south>\r\n",bm->south);
	fprintf(kml, "<east>%f</east>\r\n",bm->east);
	fprintf(kml, "<west>%f</west>\r\n",bm->west);
	fprintf(kml, "<rotation>0</rotation>\r\n");
	fprintf(kml, "</LatLonBox>\r\n");
	fprintf(kml, "</GroundOverlay>\r\n");
	fprintf(kml, "</kml>\r\n");

	fclose(kml);

	return 1;
}


int LoadPreset(OPTIONS *options, char *preset)
{
	#define PRESET_COUNT 46
	#define MAX_PRESET_LENGTH 32

	char preset_name[PRESET_COUNT][MAX_PRESET_LENGTH];
	double preset_north[PRESET_COUNT];
	double preset_south[PRESET_COUNT];
	double preset_west[PRESET_COUNT];
	double preset_east[PRESET_COUNT];
	int i;

	//This way's going to be easier to convert to an external file
	strcpy(preset_name[0],"nz"); preset_north[0]=-34;	preset_south[0]=-47.5;	preset_west[0]=166; preset_east[0]=178.5;
strcpy(preset_name[1],"northisland"); preset_north[1]=-34.37;	preset_south[1]=-41.62;	preset_west[1]=172.6; preset_east[1]=178.6;
strcpy(preset_name[2],"auckland"); preset_north[2]= -36.7;	preset_south[2]= -37.1;	preset_west[2]=174.5; preset_east[2]=175;
strcpy(preset_name[3],"aucklandcentral"); preset_north[3]=-36.835;	preset_south[3]=-36.935;	preset_west[3]=174.69; preset_east[3]=174.89;
strcpy(preset_name[4],"tauranga"); preset_north[4]=-37.6;	preset_south[4]=-37.76;	preset_west[4]=176.07; preset_east[4]=176.36;
strcpy(preset_name[5],"wellington"); preset_north[5]=-41.06;	preset_south[5]=-41.4;	preset_west[5]=174.6; preset_east[5]=175.15;
strcpy(preset_name[6],"christchurch"); preset_north[6]=-43.43;	preset_south[6]=-43.62;	preset_west[6]=172.5; preset_east[6]=172.81;
strcpy(preset_name[7],"queenstown"); preset_north[7]=-44.5;	preset_south[7]=-45.6;	preset_west[7]=168; preset_east[7]=169.5;
strcpy(preset_name[8],"dunedin"); preset_north[8]=-45.7;	preset_south[8]=-45.95;	preset_west[8]=170.175; preset_east[8]=170.755;
strcpy(preset_name[9],"au"); preset_north[9]= -10.5;	preset_south[9]= -44;	preset_west[9]=112; preset_east[9]=154;
strcpy(preset_name[10],"queensland"); preset_north[10]= -9.5;	preset_south[10]= -29;	preset_west[10]=138; preset_east[10]=154;
strcpy(preset_name[11],"sydney"); preset_north[11]= -33.57;	preset_south[11]= -34.14;	preset_west[11]=150.66; preset_east[11]=151.35;
strcpy(preset_name[13],"asia"); preset_north[13]= 58;	preset_south[13]=-11;	preset_west[13]= 67; preset_east[13]=155;
strcpy(preset_name[14],"hk"); preset_north[14]= 23.2;	preset_south[14]=21.8;	preset_west[14]=112.8; preset_east[14]=114.7;
strcpy(preset_name[15],"sg"); preset_north[15]= 1.51;	preset_south[15]= 1.15;	preset_west[15]=103.6; preset_east[15]=104.1;
strcpy(preset_name[16],"in"); preset_north[16]= 37;	preset_south[16]=6;	preset_west[16]=67.65; preset_east[16]=92.56;
strcpy(preset_name[17],"jp"); preset_north[17]=45.75;	preset_south[17]=30.06;	preset_west[17]=128.35; preset_east[17]=149.09;
strcpy(preset_name[19],"europe"); preset_north[19]= 55;	preset_south[19]= 36;	preset_west[19]=-10; preset_east[19]=32;
strcpy(preset_name[20],"es"); preset_north[20]= 44;	preset_south[20]= 35;	preset_west[20]=-10.0; preset_east[20]=5;
strcpy(preset_name[21],"it"); preset_north[21]= 47;	preset_south[21]= 36.5;	preset_west[21]=6.6; preset_east[21]=19;
strcpy(preset_name[22],"venice"); preset_north[22]=  45.6;	preset_south[22]=  45.3;	preset_west[22]=12.1; preset_east[22]=12.6;
strcpy(preset_name[23],"fr"); preset_north[23]=  51.2;	preset_south[23]=  42.2;	preset_west[23]=-5.5; preset_east[23]=8.5;
strcpy(preset_name[24],"paris"); preset_north[24]=49.1;	preset_south[24]=48.5;	preset_west[24]= 1.8; preset_east[24]= 2.8;
strcpy(preset_name[25],"uk"); preset_north[25]=  60;	preset_south[25]=  50;	preset_west[25]=-10.5; preset_east[25]=2;
strcpy(preset_name[26],"scandinaviabaltic"); preset_north[26]= 71.5;	preset_south[26]= 53.5;	preset_west[26]=4.3; preset_east[26]=41.7;
strcpy(preset_name[27],"is"); preset_north[27]= 66.6;	preset_south[27]= 63.2;	preset_west[27]=-13.5; preset_east[27]=-24.6;
strcpy(preset_name[28],"cz"); preset_north[28]= 51.1;	preset_south[28]= 48.5;	preset_west[28]=12; preset_east[28]=18.9;
strcpy(preset_name[29],"prague"); preset_north[29]= 50.178;	preset_south[29]= 49.941;	preset_west[29]=14.246; preset_east[29]=14.709;
strcpy(preset_name[30],"vienna"); preset_north[30]= 48.3;	preset_south[30]= 48.12;	preset_west[30]=16.25; preset_east[30]=16.55;
strcpy(preset_name[32],"turkeygreece"); preset_north[32]=  42.294;	preset_south[32]= 34.455;	preset_west[32]=19.33; preset_east[32]=45.09;
strcpy(preset_name[33],"istanbul"); preset_north[33]= 41.3;	preset_south[33]=  40.7;	preset_west[33]=28.4; preset_east[33]=29.7;
strcpy(preset_name[34],"middleeast"); preset_north[34]= 42;	preset_south[34]=  12;	preset_west[34]=25; preset_east[34]=69;
strcpy(preset_name[35],"uae"); preset_north[35]=  26.5;	preset_south[35]=  22.6;	preset_west[35]=51.5; preset_east[35]=56.6;
strcpy(preset_name[36],"dubai"); preset_north[36]=  25.7;	preset_south[36]=  24.2;	preset_west[36]=54.2; preset_east[36]=55.7;
strcpy(preset_name[37],"israeljordan"); preset_north[37]= 33.4;	preset_south[37]= 29.1;	preset_west[37]=34; preset_east[37]=39.5;
strcpy(preset_name[41],"usane"); preset_north[41]= 47.5;	preset_south[41]= 36.5;	preset_west[41]=-82.7; preset_east[41]=-67;
strcpy(preset_name[42],"usa"); preset_north[42]= 49;	preset_south[42]= 24;	preset_west[42]=-125; preset_east[42]=-67;
strcpy(preset_name[43],"boston"); preset_north[43]= 42.9;	preset_south[43]= 42;	preset_west[43]=-71.9; preset_east[43]=-70.5;
strcpy(preset_name[44],"arkansas"); preset_north[44]= 36.5;	preset_south[44]= 33;	preset_west[44]=-94.6; preset_east[44]=-89;
strcpy(preset_name[45],"lasvegas"); preset_north[45]= 36.35;	preset_south[45]= 35.9;	preset_west[45]=-115.35; preset_east[45]=-114.7;


	for (i=0;i<PRESET_COUNT;i++)	{
		if (!stricmp(preset,preset_name[i]))	{
			options->north=preset_north[i];
			options->south=preset_south[i];
			options->west=preset_west[i];
			options->east=preset_east[i];
		}
	}

	return 1;
}


int MakeProperFilename(char *targetstring, char *source, char *def, char *ext)
{
	if (!source)	{
		sprintf(targetstring,"%s",def);
		return 0;
	}

	if (strrchr(source, '.'))	{
		sprintf(targetstring,"%s",source);
	}
	else
		sprintf(targetstring,"%s.%s",source, ext);

	return 1;
}
