#define VERSION 0.01

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
	int xsize;
	int ysize;
	int channels;	//1, 3 or 4
	char *bitmap;
	int sizebitmap;
};
typedef struct sBitmap BM;

struct sLocation	{
	double latitude;
	double longitude;
	long timestampS; //we'll use a long instead of the high precision of google
	int accuracy;
};
typedef struct sLocation LOCATION;

int ReadLocation(FILE *json, LOCATION *location);

BM* bitmapInit(int xsize, int ysize, int channels);
int bitmapPixelSet(BM* bm, int x, int y, COLOUR c);
int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, COLOUR c);
int bitmapWrite(BM* bm, char *filename);			//this writes a .raw file, can be opened with photoshop. Not req now using PNG
int bitmapDestroy(BM* bm);

int mixColours(COLOUR *cCanvas, COLOUR *cBrush);	//canvas gets written to

COLOUR HsvToRgb(unsigned char h, unsigned char s,unsigned char v);
COLOUR TimestampToRgb(long ts, long min, long max);

int LatLongToXY(double phi, double lambda, POINT *p, double scale);	//lat, long, output point

double ipart(double x);
double round(double x);
double fpart(double x);
double rfpart(double x);
int plot(BM* bm, int x, int y, double cdouble, COLOUR c);

void Intro(char *programName);	//blurb
void Usage(char *programName); 	//called if run without arguments
//int HandleOptions(int argc,char *argv[]);

void Intro(char *programName)
{
	fprintf(stderr, "World Tracker  - Version %2.2f\r\n", VERSION);
	fprintf(stderr, "Plots out your Google Location history\r\n\r\n");
    fprintf(stderr, "Copyright © 2014 Tristan Burtenshaw\r\n");
	fprintf(stderr, "New BSD Licence (\'%s --copyright' for more information.)\r\n", programName);
	fprintf(stderr, "Contains LodePNG library by Lode Vandevenne (http://lodev.org/lodepng/)\r\n\r\n");
}

void Usage(char *programName)
{
	fprintf(stderr, "Usage: %s [-s <scale>] [input.json] [output.png]\r\n\r\n", programName);
	fprintf(stderr, "Default input: \'LocationHistory.json\' from the current folder.\r\n");
	fprintf(stderr, "Default output: \'trips.png\'.\r\n");
}

/* returns the index of the first argument that is not an option; i.e.
   does not start with a dash or a slash
*/
int HandleOptions(int argc,char *argv[], double *scale)
{
	int i,firstnonoption=0;

	for (i=1; i< argc;i++) {
		if (argv[i][0] == '/' || argv[i][0] == '-') {
			switch (argv[i][1]) {
				/* An argument -? means help is requested */
				case '?':
					Usage(argv[0]);
					break;
				case 'h':
				case 'H':
					if (!stricmp(argv[i]+1,"help")) {
						Usage(argv[0]);
						break;
					}
					/* If the option -h means anything else
					 * in your application add code here
					 * Note: this falls through to the default
					 * to print an "unknow option" message
					*/
				case 's':
				case 'S':
					if (i+1<argc)	{
						*scale = strtod(argv[i+1], NULL);
						if (strstr(argv[i+1], ".json"))	{	//if they've got a filename after the scale, then it's a mistake
							i--;							//so we'll rewind an argument
						}
						i++;	//move to the next variable, we've got a scale
						//fprintf(stderr, "Scale: %f\r\n",*scale);
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
	double scale;
	COLOUR c;

	int arg;

	FILE *csv;
	FILE *json;
	char *jsonfilename;
	char *pngfilename;

	double phi;		//lat
	double lambda;	//longit

	int xi;
	int yi;
	int oldx,oldy;
	int dx,dy;
	int yintersect;
	double m;

	int error;
	int i;
	int tempint;
	int swappedflag;
	int griddegrees;

	POINT p;
	LOCATION coord;


	Intro(argv[0]);

	jsonfilename="LocationHistory.json";	//default
	pngfilename="trips.png";	//default
	if (argc == 1) {	//if there's no arguments, then we can just use defaults
		Usage(argv[0]);
	}
	else	{			//otherwise better handle the inputs
		arg = HandleOptions(argc, argv, &scale);
		fprintf(stderr, "Input file: %s\r\n", argv[arg]);
		jsonfilename=argv[arg];
		if (arg<argc+1)	{	//output file
			fprintf(stderr, "Output file: %s\r\n", argv[arg+1]);
			pngfilename=argv[arg+1];
		}
	}

	//Open the input file
	json=fopen(jsonfilename,"r");
	if (json==NULL)	{
		printf("\r\nUnable to open \'%s\'.\r\n", jsonfilename);
		perror("Error");
		fprintf(stderr, "\r\n");
		return 1;
	}

	//Set the scale
	if ((scale<0.1) || (scale>55))	{	//have to decide the limit, i've tested to 55
		if (scale) fprintf(stderr, "Scale must be between 0.1 and 55\r\n");	//if they've entered something silly
		scale = 15;
	}
	fprintf(stdout, "Scale: %4.2f\r\n", scale);


	//Initialise the bitmap
	mainBM = bitmapInit(360*scale,180*scale,4);

	//Draw grid
	griddegrees=15;
	c.R=192;c.G=192;c.B=192;c.A=128;
	for (i=griddegrees*scale;i<180*scale;i+=griddegrees*scale)	{	//latitude deg*scale
		bitmapLineDrawWu(mainBM, 0,i,scale*360,i,c);
	}

		for (i=griddegrees*scale;i<360*scale;i+=griddegrees*scale)	{	//longit deg*scale
		bitmapLineDrawWu(mainBM, i,0, i, scale*180,c);
	}

	//Set colour
	c.R=255;	c.G=205; 	c.B=0;	c.A=255;

	oldx=-1;
	oldy=-1;

	while (ReadLocation(json, &coord)==1)	{
		//Set the colour base on time (hardcoded so far)
		c=TimestampToRgb(coord.timestampS, 1336886768, 1406220100);
		LatLongToXY(coord.latitude, coord.longitude, &p, scale);

		xi=p.x;
		yi=p.y;


		if (oldx>=0)	{	//This will be true except for very first point
			//Handle wrapping around the x-coord of map (we don't worry about the poles)
			dx=xi-oldx;
			dy=yi-oldy;
			if ((abs(dx)>180*scale))	{	//they've moved over half the map
				swappedflag=0;	//flag
				if (dx >0)	{	//if it's the eastward direction then we'll swap vars
					//printf("Pretransform %i %i to %i %i, diff:%i %i c: %i %i\r\n", oldx, oldy, xi,yi,dx,dy, 0,yintersect);
					i=xi;xi=oldx;oldx=i;	//swap x
					i=yi;yi=oldy;oldy=i;	//swap y

					dx=xi-oldx;	dy=yi-oldy;
					swappedflag=1;	//flag to ensure we swap coords back
				}

				dx=xi-(oldx-360*scale);	//oldx-360*scale is our new origin
				m=dy;	m/=dx;				//gradient, done this ugly way as doing division on ints

				yintersect = oldy + (360*scale-oldx)*m;	//still not sure this is correct
//				printf("from %i %i to %i %i, diff:%i %i yint: %i m:%f\r\n", oldx, oldy, xi,yi,dx,dy, yintersect,m);

				//We draw two separate lines
				bitmapLineDrawWu(mainBM, oldx,oldy,360*scale-1, yintersect, c);
				bitmapLineDrawWu(mainBM, 0, yintersect, xi,yi, c);

				if (swappedflag==1)	{	//swap this back if we swapped!
					i=xi;xi=oldx;oldx=i;
					i=yi;yi=oldy;oldy=i;
				}

			}
			//Here is just the normal line from oldpoint to new one
			else	{
				bitmapLineDrawWu(mainBM, oldx,oldy,xi,yi,c);		//the normal case
			}
		}
		oldx=xi;oldy=yi;

	}

	fclose(json);

	//Write the PNG file
	fprintf(stdout, "Writing to %s.", pngfilename);
	error = lodepng_encode32_file(pngfilename, mainBM->bitmap, mainBM->xsize, mainBM->ysize);
	if(error) printf("LodePNG error %u: %s\n", error, lodepng_error_text(error));

	bitmapDestroy(mainBM);
	return 0;
}


int ReadLocation(FILE *json, LOCATION *location)
{
	int step;
	char buffer[256];
	char *x;
	char *y;

	step=0;
	while (fgets(buffer, 256, json) != NULL)	{
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
				//printf("long %s",x);

				location->longitude=strtod(x, NULL)/10000000;
				return 1;
			}
		}


	}
	fprintf (stdout, "Finished reading JSON file.\r\n");
	return 0;
};


BM* bitmapInit(int xsize, int ysize, int channels)
{
	BM *temp;
	char* bitmap;

	temp=(BM*)malloc(sizeof(BM));

	temp->xsize=xsize;
	temp->ysize=ysize;
	temp->channels= channels;
	temp->sizebitmap=xsize*ysize*channels;

	bitmap=(char*)malloc(temp->sizebitmap);
	memset(bitmap, 0, temp->sizebitmap);

	printf("Bitmap at: %i. X: %i, Y: %i, channels:%i\r\n", (long)bitmap, xsize, ysize, channels);

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

	if (x>bm->xsize-1)	return 0;
	if (y>bm->ysize-1)	return 0;
	if (x<0)	return 0;
	if (y<0)	return 0;

	//Greyscale
	if (bm->channels == 1)	{
		currentc = bm->bitmap[x+y* bm->xsize *1];	//this'll be between 0 and 255

		//printf("currentc %i\r\n", currentc);

		k=(c.R+c.G+c.B)*(c.A)+currentc*3*(255-c.A);
		k=k/(255*3);
		currentc=(char)k;

		//printf("currentc %i\r\n", currentc);

		//printf("x %i y %i %i %i %i\r\n",x,y,bm->xsize, bm->channels, c.A);
		//printf("bm %i", (long)bm->bitmap);
		memset(bm->bitmap + (x + y * bm->xsize) * 1,currentc,1);	//the *1 is the channels
	}

	if (bm->channels >= 3)	{	//3 or 4 channels
		currentC.R = bm->bitmap[(x+y* bm->xsize) *bm->channels];
		currentC.G = bm->bitmap[(x+y* bm->xsize) *bm->channels+1];
		currentC.B = bm->bitmap[(x+y* bm->xsize) *bm->channels+2];
		if (bm->channels == 4)
			currentC.A = bm->bitmap[(x+y* bm->xsize) *4+3];
		else
			currentC.A=255;

		if (mixColours(&currentC, &c))	{
			memset(bm->bitmap + (x + y * bm->xsize) * bm->channels+0,currentC.R,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->xsize) * bm->channels+1,currentC.G,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->xsize) * bm->channels+2,currentC.B,1);	//the *4 is the channels
			if (bm->channels == 4)
				memset(bm->bitmap + (x + y * bm->xsize) * 4+3,currentC.A,1);	//the *4 is the channels
		}
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
	int tempint;
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
	int step;
	//first correct it so going small to big
	//if (x1<x0)	{tempdouble=x0;x0=x1;x1=tempdouble; tempdouble=y0;y0=y1;y1=tempdouble;}


	//based on the wikipedia article
//	printf("Line: %f,%f to %f,%f [R%iG%iB%i]", x0,y0,x1,y1, c.R, c.G, c.B);
	steep=(abs(y1 - y0) > abs(x1 - x0));

	if (steep)	{
		tempdouble = x0; x0=y0; y0=tempdouble;
		tempdouble = x1; x1=y1; y1=tempdouble;
	}

	if (x0>x1)	{
		tempdouble = x1; x1=x0; x0=tempdouble;	//swap x0,x1
		tempdouble = y1; y1=y0; y0=tempdouble;  //swap y0,y1
//		printf(" (swapped)");

//		printf("{now %f,%f to %f,%f}", x0,y0,x1,y1);
	}

//	printf("\r\n");
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

//	 step=1;
	 //if (xpxl1>xpxl2)	{
//		 tempdouble=xpxl1;xpxl1=xpxl2;xpxl2=tempdouble;
//		}
	//	 step=-1;
	if (xpxl1>xpxl2)	{
		 printf("[%f should be smaller than %f]\r\n",xpxl1,xpxl2);
		}
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
         intery = intery + gradient;
		}




	return 0;
}

int bitmapWrite(BM* bm, char *filename)
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
	return floor(x);
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
		 	bitmapPixelSet(bm, x, y, c);

//printf("x %i y%i c%f \r\n",x,y,doublec);


return 0;
}

int LatLongToXY(double phi, double lambda, POINT *p, double scale)
{
	p->y=(90-phi)*scale+0.5;
	p->x=(lambda+180)*scale+0.5;

	if (p->x >= 360*scale)
		p->x=0;

	if (p->y >= 360*scale)
		p->y=0;

//	printf("%i %i\r\n",p->x,p->y);

	return 0;
}



COLOUR HsvToRgb(unsigned char h, unsigned char s,unsigned char v)
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
	return HsvToRgb((char)hue,255,255);
}
