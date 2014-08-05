//current version

#define VERSION 0.11

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "lodepng.h"

#define PI 3.14159265
struct sPoint	{
	int x;
	int y;
};
typedef struct sPoint POINT;

struct sRGBAColour	{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};
typedef struct sRGBAColour COLOUR;

struct sBitmap	{
	int width;
	int height;

	double west;
	double east;
	double north;
	double south;

	double zoom;	//on a full map, this is the number of pixels per degree

	char *bitmap;	//always going to be a four channel RGBA bitmap now
	int sizebitmap;
};
typedef struct sBitmap BM;

struct sLocation	{
	double latitude;
	double longitude;
	long timestampS; //we'll use a long instead of the high precision of google
	int accuracy;

	struct sLocation* next;	//linked list
	struct sLocation* prev;
	struct sLocation* next1ppd;
	struct sLocation* next10ppd;
	struct sLocation* next100ppd;
};
typedef struct sLocation LOCATION;

struct sLocationHistory	{
	FILE *json;
	char *jsonfilename;

	unsigned long	numPoints;
	unsigned long	earliesttimestamp;
	unsigned long	latesttimestamp;
	LOCATION * first;
	LOCATION * last;
};
typedef struct sLocationHistory LOCATIONHISTORY;

int LoadLocations(LOCATIONHISTORY *lh);
int FreeLocations(LOCATIONHISTORY *locationHistory);
int ReadLocation(LOCATIONHISTORY *lh, LOCATION *location);

BM* bitmapInit(int width, int height, double zoom, double north, double south, double west, double east);
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
	fprintf(stdout, "Usage: %s [-z <zoom>] [input.json] [output.png]\r\n\r\n", programName);
	fprintf(stdout, "Default input: \'LocationHistory.json\' from the current folder.\r\n");
	fprintf(stdout, "Default output: \'trips.png\'.\r\n");
}

/* returns the index of the first argument that is not an option; i.e.
   does not start with a dash or a slash
*/
int HandleOptions(int argc,char *argv[], double *zoom)
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
						break;
					}
					/* If the option -h means anything else
					 * in your application add code here
					 * Note: this falls through to the default
					 * to print an "unknow option" message
					*/
				case 'z':
				case 'Z':
					if (i+1<argc)	{
						*zoom = strtod(argv[i+1], NULL);
						if (strstr(argv[i+1], ".json"))	{	//if they've got a filename after the zoom, then it's a mistake
							i--;							//so we'll rewind an argument
						}
						i++;	//move to the next variable, we've got a zoom
						//fprintf(stderr, "Zoom: %f\r\n",*zoom);
					}
					break;
				default:
					fprintf(stderr,"unknown option %s\n",argv[i]);
					break;
			}
		}
		else {
			firstnonoption = i;
			break;
		}
	}
	return firstnonoption;
}

int main(int argc,char *argv[])
{
	BM*	mainBM;

	double west;
	double east;
	double north;
	double south;

	int height, width;
	int testwidth;
	double zoom;
	COLOUR c;

	int arg;

//	FILE *csv;

	LOCATIONHISTORY locationHistory;

	//FILE *json;
	//char *jsonfilename;
	char *pngfilename;

	unsigned long countPoints;
	double oldlat, oldlon;

	int error;
	int griddegrees;

	POINT p;
	LOCATION *coord;

	//Display introductory text
	PrintIntro(argv[0]);

	locationHistory.jsonfilename="LocationHistory.json";	//default
	pngfilename="trips.png";	//default
	if (argc == 1) {	//if there's no arguments, then we can just use defaults
		PrintUsage(argv[0]);
	}
	else	{			//otherwise better handle the inputs
		arg = HandleOptions(argc, argv, &zoom);
		if (arg>0)	{
			fprintf(stdout, "Input file: %i %s\r\n", arg, argv[arg]);
			locationHistory.jsonfilename=argv[arg];
			if (arg<argc+1)	{	//output file
				fprintf(stdout, "Output file: %s\r\n", argv[arg+1]);
				pngfilename=argv[arg+1];
			}
		}
	}

	//Open the input file
	locationHistory.json=fopen(locationHistory.jsonfilename,"r");
	if (locationHistory.json==NULL)	{
		fprintf(stderr, "\r\nUnable to open \'%s\'.\r\n", locationHistory.jsonfilename);
		perror("Error");
		fprintf(stderr, "\r\n");
		return 1;
	}

	//Set the zoom
	if ((zoom<0.1) || (zoom>55))	{	//have to decide the limit, i've tested to 55
		if (zoom) fprintf(stderr, "Zoom must be between 0.1 and 55\r\n");	//if they've entered something silly
		zoom = 0;	//set to zero, then we'll calculate.
	}

	height=0;
	width =0;

	if ((height==0) && (width == 0))	{	//if no height or width specified, we'll base it on zoom (or default zoom)
		if (zoom==0)	{zoom=15;}	//default zoom
		width=360*zoom;
		height= 180*zoom;
	}	else
	if (width==0)	{

	}

	west=-180;
	east=180;
	north=90;
	south=-90;


	//Auckland
	west=174.5;
	east=175;
	north = -36.7;
	south = -37.1;

	//if the aspect ratio of coords is different, set the width to be related to the
	testwidth=height*(east-west)/(north-south);
	printf("tw: %i, w: %i\r\n", testwidth, width);
	if (testwidth != width)	{
		width=testwidth;
	}


	fprintf(stdout, "Zoom: %4.2f\r\n", zoom);



	//Initialise the bitmap
	mainBM = bitmapInit(width, height, zoom, north, south, west, east);

	//Draw grid
	griddegrees=1;
	c.R=192;c.G=192;c.B=192;c.A=128;
	DrawGrid(mainBM, griddegrees, c);
	//ColourWheel(mainBM, 100, 100, 100, 5);  	//Color wheel test of lines and antialiasing

	oldlat=0;oldlon=0;
	locationHistory.earliesttimestamp=-1;
	locationHistory.latesttimestamp=0;
	countPoints=0;

	LoadLocations(&locationHistory);

	coord=locationHistory.first;
	while (coord)	{
		c=TimestampToRgb(coord->timestampS, 1300000000, 1300000000+(60*60*24*183));	//about 6 month cycle

		if (coord->accuracy <200000)	{
			//draw a line from last point to the current one.
			if (countPoints>0)	{
				bitmapCoordLine(mainBM, coord->latitude, coord->longitude, oldlat, oldlon, c);
			}

			oldlat=coord->latitude;oldlon=coord->longitude;
			countPoints++;
		}
		coord=coord->next;
	}

	fprintf(stdout, "Ploted: %i points\r\n", countPoints);
	fprintf(stdout, "Earliest: %i\tLastest: %i\r\n", locationHistory.earliesttimestamp, locationHistory.latesttimestamp);
	printf("From:\t%s\r\n", asctime(localtime(&locationHistory.earliesttimestamp)));
	printf("To:\t%s\r\n", asctime(localtime(&locationHistory.latesttimestamp)));

	fclose(locationHistory.json);
	FreeLocations(&locationHistory);


	//Write the PNG file
	fprintf(stdout, "Writing to %s.", pngfilename);
	error = lodepng_encode32_file(pngfilename, mainBM->bitmap, mainBM->width, mainBM->height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));

	bitmapDestroy(mainBM);
	return 0;
}


int LoadLocations(LOCATIONHISTORY *locationHistory)
{
	LOCATION *coord;
	LOCATION *prevCoord;

	prevCoord=NULL;
	coord=malloc(sizeof(LOCATION));
	locationHistory->first = coord;
	while (ReadLocation(locationHistory, coord)==1)	{
		if (coord->timestampS > locationHistory->latesttimestamp)
			locationHistory->latesttimestamp = coord->timestampS;
		if (coord->timestampS<locationHistory->earliesttimestamp)
			locationHistory->earliesttimestamp = coord->timestampS;
		locationHistory->numPoints++;

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


BM* bitmapInit(int xsize, int ysize, double zoom, double north, double south, double west, double east)
{
	BM *temp;
	char* bitmap;

	temp=(BM*)malloc(sizeof(BM));

	temp->width=xsize;
	temp->height=ysize;
	temp->sizebitmap=xsize*ysize*4;
	temp->zoom=zoom;
	temp->north=north;
	temp->south=south;
	temp->west=west;
	temp->east=east;

	bitmap=(char*)calloc(temp->sizebitmap, sizeof(char));
	//memset(bitmap, 0, temp->sizebitmap);

	printf("New bitmap width: %i, height: %i\r\n", xsize, ysize);

	temp->bitmap=bitmap;

	return temp;
}

int bitmapDestroy(BM *bm)
{
	free(bm->bitmap);
	free(bm);
	return 0;
}

int bitmapPixelSet(BM* bm, int x, int y, COLOUR c)
{
	unsigned char currentc;

	COLOUR currentC;

	int k;

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
	double xgap,ygap;
	double xpxl1, ypxl1;
	double xpxl2, ypxl2;

	double x;

	double doublec;

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



	return 0;
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
	COLOUR rgba;
	double hue;
	long diff;

	diff=max-min;
	hue=(ts-min);
	hue=hue/diff;
	hue=hue*255;

//	printf("%i\t%i\t%i\t%f\r\n", ts, diff, ts-min, hue);
	return HsvToRgb((char)hue,255,255,255);
}


int DrawGrid(BM* bm, int spacing, COLOUR c)
{
	//int i;
	int lat, lon;
	double x1,y1;
	double x2,y2;

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
