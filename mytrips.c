#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "lodepng.h"
#include "mytrips.h"

COLOUR cpm[12];
COLOUR cpd[7];


int PlotPaths(BM* bm, LOCATIONHISTORY *locationHistory, OPTIONS *options)
{
	COLOUR c;
	double oldlat, oldlon;
	LOCATION *coord;


	if (locationHistory==NULL)
		return 0;
	oldlat=1000;oldlon=1000;
	bm->countPoints=0;

	options->zoom=options->width/(options->nswe.east-options->nswe.west);
	bm->zoom = options->zoom;

	coord=locationHistory->first;

	while (coord)	{
		//Set the colour to draw the line.
		c.A=options->alpha;
		if (options->colourby == COLOUR_BY_TIME)	{
			c = TimestampToRgb(coord->timestampS, options->colourcycle);		//based on timestamp
		}
		else if (options->colourby == COLOUR_BY_SPEED)	{
			if (coord->secondsfromprev!=0)	{
				c = SpeedToRgb(coord->distancefromprev/(double)coord->secondsfromprev, 30);
			}
			else {c.R=255; c.G=255; c.B =255; c.A=255;}
		}
		else if (options->colourby == COLOUR_BY_ACCURACY)	{
			c=AccuracyToRgb(coord->accuracy);
		}
		else if (options->colourby == COLOUR_BY_DAYOFWEEK)	{
			c=DayOfWeekToRgb(coord->timestampS, options->colourextra);
		}
		else if (options->colourby == COLOUR_BY_HOUR)	{
			c=HourToRgb(coord->timestampS, NULL, NULL);
		}
		else if (options->colourby == COLOUR_BY_MONTH)	{
			c=MonthToRgb(coord->timestampS, options->colourextra);
		}


		if (coord->accuracy <200000 && (coord->timestampS >= options->fromtimestamp) && (coord->timestampS <= options->totimestamp))	{
			//draw a line from last point to the current one.
			bm->countPoints+= bitmapCoordLine(bm, coord->latitude, coord->longitude, oldlat, oldlon,options->thickness, &c);
			oldlat=coord->latitude;oldlon=coord->longitude;
		}

		//Move onto the next appropriate coord
		//?i'll move this outside the loop for speed reasons
		if (bm->zoom>10000)	coord=coord->next;
		else if (bm->zoom>1000) coord=coord->next10000ppd;
		else if (bm->zoom>100) coord=coord->next1000ppd;
		else if (bm->zoom>10) coord=coord->next100ppd;
		else if (bm->zoom>1) coord=coord->next10ppd;
		else coord=coord->next1ppd;

	}

	return 0;
}

int LoadLocations(LOCATIONHISTORY *locationHistory, char *jsonfilename, void(*progressfn)(int))
{
	LOCATION *coord;
	LOCATION *prevCoord;

	LOCATION *waitingFor1;
	LOCATION *waitingFor10;
	LOCATION *waitingFor100;
	LOCATION *waitingFor1000;
	LOCATION *waitingFor10000;


	//Initialise some statistics
	locationHistory->earliesttimestamp=-1;
	locationHistory->latesttimestamp=0;


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
	waitingFor10000 = coord;

	int progress=0;
	long twofiftysixth;
	fseek(locationHistory->json,0, SEEK_END);
	locationHistory->filesize = ftell(locationHistory->json);
	rewind(locationHistory->json);
	locationHistory->filepos=0;

	twofiftysixth = locationHistory->filesize/256;

	while (ReadLocation(locationHistory, coord)==1)	{

		if (progressfn)	{
			if (locationHistory->filepos > twofiftysixth * progress)	{
				progress++;
				(*progressfn)(progress);
			}
		}


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

		if ((fabs(waitingFor10000->latitude - coord->latitude) >0.0001) ||(fabs(waitingFor10000->latitude - coord->latitude) >0.0001))	{
			waitingFor10000->next10000ppd = coord;
			waitingFor10000=coord;
		}


		//Distance, speed, deltaT calculations here (Might just have distance, and time change, then calculate speed when required).
		if (prevCoord)	{
			coord->distancefromprev = MetersApartFlatEarth(prevCoord->latitude, prevCoord->longitude, coord->latitude, coord->longitude);
			coord->secondsfromprev = coord->timestampS - prevCoord->timestampS;
//		printf("%fm (%i to %i) %is\t",coord->distancefromprev, prevCoord->timestampS, coord->timestampS, coord->secondsfromprev);
		}
		else	{
			coord->distancefromprev = 0;
			coord->secondsfromprev = 0;
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

/* TEMPORARY */
	TRIP * trip;
	NSWE home;
	NSWE work;

	home.north=-36.843975;
	home.south=-36.857656;
	home.west=174.757584;
	home.east=174.774074;

	work.north=-37.015956;
	work.south=-37.025932;
	work.west=174.889670;
	work.east=174.903095;



//	trip = GetLinkedListOfTrips(&home, &work, locationHistory);
//	ExportTripData(trip, "histogram.txt");
//	FreeLinkedListOfTrips(trip);

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
		lh->filepos+=strlen(buffer);
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


int bitmapInit(BM* bm, OPTIONS* options, LOCATIONHISTORY *lh)
{
	char* bitmap;

	bm->width=options->width;
	bm->height=options->height;
	bm->sizebitmap=options->width*options->height*4;
	bm->zoom=options->zoom;

	bitmap=(char*)calloc(bm->sizebitmap, sizeof(char));

//	printf("\r\nBitmap initiated %i. Width: %i, height: %i", bitmap, options->width, options->height);

	bm->bitmap=bitmap;
	bm->options=options;
	bm->lh=lh;

	return 1;
}

int bitmapDestroy(BM *bm)
{
	free(bm->bitmap);
	bm->bitmap = NULL;
	return 0;
}

COLOUR bitmapPixelGet(BM* bm, int x, int y)
{
	COLOUR c;

	c.R = bm->bitmap[(x+y* bm->width) *4];
	c.G = bm->bitmap[(x+y* bm->width) *4+1];
	c.B = bm->bitmap[(x+y* bm->width) *4+2];
	c.A = bm->bitmap[(x+y* bm->width) *4+3];


	return c;
}

int bitmapPixelSet(BM* bm, int x, int y, COLOUR *c)
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

	if (mixColours(&currentC, c))	{
			memset(bm->bitmap + (x + y * bm->width) * 4+0,currentC.R,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+1,currentC.G,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+2,currentC.B,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+3,currentC.A,1);	//the *4 is the channels
	}



	return 1;
}

int mixColours(COLOUR *cCanvas, COLOUR *cBrush)	//this alters the canvas
{

	int r,g,b,a;


	if ((cBrush->A)==0)	{//if we're painting with a transparent brush, just return
		return 1;
	}

//	if ((cBrush->A)==255)	{	//if the brush is completely opaque
//		cCanvas->R = cBrush->R;
//		cCanvas->G = cBrush->G;
//		cCanvas->B = cBrush->B;
//		cCanvas->A = 255;
//		return 1;
//	}

	//first work out the output alpha
	a=cBrush->A + (cCanvas->A * (255 - cBrush->A))/255;	//proper way to do it, it's not simply additive
	if (a>255)
			a = 255;

	if (a>0)	{	//only bother writing if there is an alpha

		//printf("r%i g%i b%i a%i + r%i g%i b%i a%i = ",cCanvas->R,cCanvas->G,cCanvas->B,cCanvas->A, cBrush->R,cBrush->G,cBrush->B,cBrush->A);

		//This is essentially doing the RGB function in https://en.wikipedia.org/wiki/Alpha_compositing
		//I rearranged equation in wolfram alpha to take care of the fact by alphas are between 0 and 255
		//And ensured that minimal divisions were done.
		r = (255*cBrush->R * cBrush->A) - (cBrush->A * cCanvas->A * cCanvas->R) + (255*cCanvas->A * cCanvas->R);
		r /=255;
		r /=a;

		g = (255*cBrush->G * cBrush->A) - (cBrush->A * cCanvas->A * cCanvas->G) + (255*cCanvas->A * cCanvas->G);
		g /=255;
		g /=a;

		b = (255*cBrush->B * cBrush->A) - (cBrush->A * cCanvas->A * cCanvas->B) + (255*cCanvas->A * cCanvas->B);
		b /=255;
		b /=a;

		if (r>255)	r=255;
		if (g>255)	g=255;
		if (b>255)	b=255;
		//printf("r%i g%i b%i a%i\r\n",r,g,b,a);

	}
	else	{	//if there's no alpha in the output, then remove all colour
		cCanvas->R=0;
		cCanvas->G=0;
		cCanvas->B=0;
		cCanvas->A=0;
		return 0;
	}


	//set the canvas
	cCanvas->R=r;
	cCanvas->G=g;
	cCanvas->B=b;
	cCanvas->A=a;
	return 1;

}

int bitmapFilledCircle(BM* bm, double x, double y, double radius, COLOUR *c)
{
	int row,col;
	double distance, diff;


	for (row=(int)(y-radius); row<y+radius+1; row++)	{
		for (col=(int)(x-radius); col<x+radius+1; col++)	{
			distance = sqrt((x-col) *(x-col) + (y-row)*(y-row));
			//diff = rsquared-distancesquared;
			//printf("diff: %.2f\tdist:%.2f\t r%i\tc%i\r\n",diff, distancesquared, row, col);
			//c.R=c.B; c.A=120;
			if (distance<=radius-1)
				plot(bm,col,row,255,c);
			else if (distance<radius)	{
				diff=(radius-distance);
				plot(bm,col,row,diff*256,c);
			}


		}
	}

	return 1;
}

int bitmapSquare(BM* bm, int x0, int y0, int x1, int y1, COLOUR *cBorder, COLOUR *cFill)
{
	int x,y;

	//Draw the border
	if (cBorder)	{
		//top border
		y=y0;
		for (x=x0;x<=x1;x++)	{
			bitmapPixelSet(bm, x,y, cBorder);
		}
		//bottom border
		y=y1;
		for (x=x0;x<=x1;x++)	{
			bitmapPixelSet(bm, x,y, cBorder);
		}
		//left border
		x=x0;
		for (y=y0;y<=y1;y++)	{
			bitmapPixelSet(bm, x,y, cBorder);
		}
		//right border
		x=x0;
		for (y=y0;y<=y1;y++)	{
			bitmapPixelSet(bm, x,y, cBorder);
		}
	}

	if (cFill)	{
		for (x=x0+1;x<x1;x++)	{
			for (y=y0+1;y<y1;y++)	{
				bitmapPixelSet(bm, x,y, cFill);
			}
		}
	}

	return 1;
}

int bitmapLineDrawWu(BM* bm, double x0, double y0, double x1, double y1, int thickness, COLOUR *c)
{
	int steep;
	double tempdouble;

	double dx,dy;
	double gradient;
	double xend,yend;
	double intery;

	double hypotenusethickness;
	int x;

	steep=(fabs(y1 - y0) > fabs(x1 - x0));	//if it's a steep line, swap the xs with the ys

	if (steep)	{
		tempdouble = x0; x0=y0; y0=tempdouble;
		tempdouble = x1; x1=y1; y1=tempdouble;
	}

	if (x0>x1)	{							//get things moving from small to big
		tempdouble = x1; x1=x0; x0=tempdouble;	//swap x0,x1
		tempdouble = y1; y1=y0; y0=tempdouble;  //swap y0,y1
	}


	dx=x1-x0;
	dy=y1-y0;
	if (dx==0)	return 0;

	gradient = dy/dx;


	//clip the lines within 0 and width (remember this may be rotated 90 deg if steep)
	if ((x0<0))	{
		y0-=gradient*x0; x0=0;
	}

	if ((x1>(steep ? bm->height: bm->width)))	{
		double yatend, yintercept;
		yintercept = y0-gradient*x0;
		yatend = gradient*(steep ? bm->height: bm->width)+yintercept;
		x1=(float)(steep ? bm->height: bm->width);y1=yatend;
	}

	if ((y0<0) && (y1<0))
		return 0;

	//these are used to construct thick lines, this is the above and the under part
	int r,f;
	hypotenusethickness=thickness*sqrt(gradient*gradient+1);
	r=0-hypotenusethickness/2;
	f=(hypotenusethickness+1)/2;


	if (abs(gradient)>1)	{
		printf("(%.1f, %.1f) (%.1f, %.1f) %.2f\t %i\t %.4f \r\n", x0,y0, x1,y1, hypotenusethickness, thickness,gradient);
	}

	//Convert to int and round;

	x0=(int)(x0+0.5);
	y0=(int)(y0+0.5);
	x1=(int)(x1+0.5);
	y1=(int)(y1+0.5);


	unsigned char rstrength, fstrength;	//the intensity of the fills

	// handle first endpoint

	/*
	if (steep)	{
		bitmapFilledCircle(bm,y0,x0,thickness/2,c);
	}
	else
		bitmapFilledCircle(bm,x0,y0,thickness/2,c);

*/

    xend = x0;
    yend = y0 + gradient * (xend - x0);
    int xpxl1 = xend;   //this will be used in the main loop
    //int ypxl1 = ipart(yend);

    intery = yend + gradient; // first y-intersection for the main loop

     // handle second endpoint
	xend = round(x1);
    yend = y1 + gradient * (xend - x1);

    int xpxl2 = x1; //this will be used in the main loop


//    int ypxl2 = ipart(yend);

	fstrength = fpart(yend) * 255;
	rstrength= 255^fstrength;


	xpxl1=x0; xpxl2=x1;
	for (x = xpxl1 + 0;x<xpxl2+1;x++)	{
		fstrength =  fpart(intery)*255;
		rstrength= 255^fstrength; //equiv of subtracting it from 255
        if  (steep)	{
            plot(bm, ipart(intery)+r, x, rstrength, c);
			for (int t=r+1;t<f;t++)	{
				plot(bm, ipart(intery)+t, x,  255, c);
			}
            plot(bm, ipart(intery)+f, x,fstrength , c);
			}
         else	{
            plot(bm, x, ipart (intery)+r, rstrength, c);
			for (int t=r+1;t<f;t++)	{
				plot(bm, x, ipart(intery)+t, 255, c);
			}
            plot(bm, x, ipart (intery)+f, fstrength, c);
			}
         intery = intery + gradient;
	}

	return 1;
}

int bitmapCoordLine(BM *bm, double lat1, double lon1, double lat2, double lon2, int thickness, COLOUR *c)
{
	double tempdouble;
	double dx,dy;
	double yintersect;
	double m;

	double x1,y1;
	double x2,y2;
	int swappedflag;


	//if it's flagged not to draw then return
	if (lon2>360 || lon1>360) return 0;
	//if the line is obviously out of the bounds of the bitmap then can return
	if ((lon1 < bm->options->nswe.west) && (lon2 < bm->options->nswe.west))	return 0;
	if ((lon1 > bm->options->nswe.east) && (lon2 > bm->options->nswe.east))	return 0;
	if ((lat1 < bm->options->nswe.south) && (lat2 < bm->options->nswe.south))	return 0;
	if ((lat1 > bm->options->nswe.north) && (lat2 > bm->options->nswe.north))	return 0;


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
		bitmapLineDrawWu(bm, x2,y2,360*bm->zoom-1, yintersect, thickness, c);
		bitmapLineDrawWu(bm, 0, yintersect, x1,y1,thickness, c);

		if (swappedflag==1)	{	//swap this back if we swapped!
			tempdouble=x1;x1=x2;x2=tempdouble;
			tempdouble=y1;y1=y2;y2=tempdouble;
		}

	}
	//Here is just the normal line from oldpoint to new one
	else	{
		//printf("bcl %f, %f to %f %f\r\n",x2,y2,x1,y1);
		return bitmapLineDrawWu(bm, x2,y2,x1,y1,thickness, c);		//the normal case
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
	int xi=x;
	return (double)xi;
}
double round(double x)
{
	//return ipart(x+0.5);
	int xi = x+0.5;
	return (double)xi;

}
double fpart(double x)
{
	return (x-ipart(x));
}
double rfpart(double x)
{
	return 1 - fpart(x);
}


int plot(BM* bm, int x, int y, unsigned char cchar, COLOUR *c)	//plot will plot a colour with an alpha (used for aliasing)
{
	if (cchar==0)	{	//don't bother if it's completely transparent
		return 0;
	}
	if (c->A==0)
		return 0;

	if (cchar==255)	{	//if the alpha is full.
		 bitmapPixelSet(bm, x, y, c);
		 return 1;
	}


	COLOUR fadedColour;
//	unsigned int cint;
//	cint = cchar;
//	cint *=c->A;
//	cint /=256;
//	fadedColour.A=(char)cint;

	fadedColour.R=c->R;
	fadedColour.G=c->G;
	fadedColour.B=c->B;
	fadedColour.A=c->A*cchar/255;


 	bitmapPixelSet(bm, x, y, &fadedColour);


return 1;
}

int LatLongToXY(BM *bm, double phi, double lambda, double *x, double *y)
{
	double width, height;
	width = bm->width;
	height = bm->height;

	*y=(bm->options->nswe.north - phi)/(bm->options->nswe.north - bm->options->nswe.south)*height;
	*x=(lambda - bm->options->nswe.west)/(bm->options->nswe.east - bm->options->nswe.west)*width;

	return 0;
}


int CreateHeatmap(BM *bm)
{
	bm->heatmap = malloc(sizeof(HEATMAP));
	bm->heatmap->heatmappixels = malloc(sizeof(unsigned int)*bm->width * bm->height);
	memset(bm->heatmap->heatmappixels, 0,sizeof(unsigned int)*bm->width * bm->height);

	bm->heatmap->maxtemp = 0;
	bm->heatmap->width = bm->width;
	bm->heatmap->height = bm->height;

	return 1;
}


int HeatmapAddPoint(HEATMAP *hm, int x, int y)
{
	hm->heatmappixels[x+y*hm->width]+=16;

	if (x>0) hm->heatmappixels[x-1+y*hm->width]+=8;
	if (y>0) hm->heatmappixels[x+(y-1)*hm->width]+=8;
	if (x<hm->height-1) hm->heatmappixels[x+1*hm->width]+=8;
	if (y<hm->width-1) hm->heatmappixels[x+(y+1)*hm->width]+=8;



	if (hm->heatmappixels[x+y*hm->width] > hm->maxtemp)	{
		hm->maxtemp = hm->heatmappixels[x+y*hm->width];
		}

	return 1;

}

int HeatmapPlot(BM* bm, LOCATIONHISTORY*lh)
{
	LOCATION *coord;

	double x,y;

	CreateHeatmap(bm);

	coord=lh->first;
	while (coord)	{
			LatLongToXY(bm,coord->latitude, coord->longitude, &x, &y);
			//printf("%i %i\r\n", (int)x,(int)y);
			if ((x>0) && (y>0) &&(x<bm->width) && (y<bm->height))	{
				HeatmapAddPoint(bm->heatmap,(int)x,(int)y);
			}
		coord=coord->next;
	}

	HeatmapToBitmap(bm);
	return 0;

}

unsigned char HeatmapIntToCharNormalisedLog(unsigned int Temp)
{

	if (Temp==0)	return 0;
	unsigned char logs0to255[256] = {0, 15, 25, 31, 37, 41, 44, 47, 50, 52, 55, 57, 58, 60, 62, 63, 65, 66, 67, 68, 70, 71, 72, 73, 74, 74, 75, 76, 77, 78, 78, 79, 80, 81, 81, 82, 83, 83, 84, 84, 85, 85, 86, 87, 87, 88, 88, 89, 89, 89, 90, 90, 91, 91, 92, 92, 92, 93, 93, 94, 94, 94, 95, 95,
95, 96, 96, 97, 97, 97, 98, 98, 98, 98, 99, 99, 99, 100, 100, 100, 101, 101, 101, 101, 102, 102, 102, 102, 103, 103, 103, 103, 104, 104, 104, 104, 105, 105, 105, 105, 106, 106, 106, 106, 107, 107, 107, 107, 107, 108, 108, 108, 108, 108, 109, 109, 109, 109, 109, 110, 110, 110, 110, 110, 111, 111, 111, 111, 111,
121, 121, 121, 121, 121, 121, 121, 121, 121, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127};
	printf("t: %i ", Temp);

	if (Temp<256)
		return logs0to255[Temp];


			return 0;
}



int HeatmapToBitmap(BM *bm)
{
	HEATMAP *hm;
	hm=bm->heatmap;
	unsigned char hue;
	double huecalc;
	double logmaxtemp;

	if (!hm->maxtemp)
		return 0;

	//logmaxtemp=		log((double)hm->maxtemp);

	hm->maxtemp = 500;
	printf(	"mt: %i", hm->maxtemp);

	for (int y=0; y<bm->height; y++)	{
		for (int x=0; x<bm->width; x++)	{
			if (hm->heatmappixels[x+y* hm->width] > 0)	{
				//huecalc = log((double)hm->heatmappixels[x+y* hm->width]) * 166.0 / logmaxtemp;
				//hue=(unsigned char)huecalc;
//				printf("%4.2f %4.2f %i\r\n", log((double)hm->heatmappixels[x+y* hm->width]), log((double)hm->maxtemp), hue);

//				bitmapPixelSet(bm, x, y, HeatmapColour(
//					HeatmapIntToCharNormalisedLog(hm->heatmappixels[x + y*hm->width] * (2^16-1)/hm->maxtemp)
//					));
			}
		}
	}

	return 1;
}


COLOUR HeatmapColour(unsigned char normalisedtemp)	{
	COLOUR rgb;
	rgb	= HsvToRgb(normalisedtemp*166/255, 255 ,255, 255);

	return rgb;
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



COLOUR TimestampToRgb(long ts, long max)
{
	double hue;

	if (max)	{		//avoid dividing by zero
		hue=ts;
		hue=hue/ max;
		hue=hue*255;
	}
	else
		hue=0;

//	printf("%i\t%i\t%i\t%f\r\n", ts, diff, ts-min, hue);
	return HsvToRgb((char)hue,255,255,255);
}


COLOUR SpeedToRgb(double speed, double maxspeed)
{
	double hue;

	hue = speed/maxspeed * 200;

	return HsvToRgb((char)hue,255,255,255);
}

COLOUR AccuracyToRgb(int accuracy)
{
	COLOUR cDarkred;
	COLOUR cOrangeRed;
	COLOUR cGold;
	COLOUR cGreenYellow;
	COLOUR cDarkgreen;

	cDarkgreen.R=0;	cDarkgreen.G=100;	cDarkgreen.B=0;

	cGreenYellow.R=173; cGreenYellow.G=255; cGreenYellow.B=47;
	cGold.R = 255;	cGold.G = 215;	cGold.B = 0;
	cOrangeRed.R=255; cOrangeRed.G=69; cOrangeRed.B=0;
	cDarkred.R=139; 	cDarkred.G=0;	cDarkred.B=0;

	if (accuracy<10)	return cDarkgreen;
	if (accuracy<50)	return cGreenYellow;
	if (accuracy<100)	return cGold;
	if (accuracy<500)	return cOrangeRed;
	return cDarkred;

}


COLOUR MonthToRgb(long ts, COLOUR *colourPerMonthArrayOfTwelve)
{
	struct tm time;
	localtime_s(&ts, &time);

	if (colourPerMonthArrayOfTwelve==NULL)	{
		cpm[0x0].R=0xF1;	cpm[0x0].G=0xD5;	cpm[0x0].B=0x45;	cpm[0x0].A=0xFF;
		cpm[0x1].R=0xCE;	cpm[0x1].G=0x8B;	cpm[0x1].B=0x22;	cpm[0x1].A=0xFF;
		cpm[0x2].R=0xA7;	cpm[0x2].G=0x45;	cpm[0x2].B=0x05;	cpm[0x2].A=0xFF;
		cpm[0x3].R=0x8C;	cpm[0x3].G=0x2F;	cpm[0x3].B=0x0C;	cpm[0x3].A=0xFF;
		cpm[0x4].R=0x79;	cpm[0x4].G=0x42;	cpm[0x4].B=0x3D;	cpm[0x4].A=0xFF;
		cpm[0x5].R=0x6A;	cpm[0x5].G=0x63;	cpm[0x5].B=0x6D;	cpm[0x5].A=0xFF;
		cpm[0x6].R=0x59;	cpm[0x6].G=0x72;	cpm[0x6].B=0x6D;	cpm[0x6].A=0xFF;
		cpm[0x7].R=0x52;	cpm[0x7].G=0x72;	cpm[0x7].B=0x3E;	cpm[0x7].A=0xFF;
		cpm[0x8].R=0x52;	cpm[0x8].G=0x75;	cpm[0x8].B=0x0C;	cpm[0x8].A=0xFF;
		cpm[0x9].R=0x69;	cpm[0x9].G=0x8D;	cpm[0x9].B=0x05;	cpm[0x9].A=0xFF;
		cpm[0xA].R=0xA9;	cpm[0xA].G=0xB9;	cpm[0xA].B=0x23;	cpm[0xA].A=0xFF;
		cpm[0xB].R=0xEA;	cpm[0xB].G=0xE3;	cpm[0xB].B=0x47;	cpm[0xB].A=0xFF;


		return cpm[time.tm_mon];
	}
	return colourPerMonthArrayOfTwelve[time.tm_mon];


}


COLOUR DayOfWeekToRgb(long ts, COLOUR *colourPerDayArrayOfSeven)
{
	struct tm time;
	localtime_s(&ts, &time);

	if (colourPerDayArrayOfSeven==NULL)	{

		cpd[0].R=0x32;	cpd[0].G=0x51;	cpd[0].B=0xA7;	cpd[0].A=0xFF;
		cpd[1].R=0xc0;	cpd[1].G=0x46;	cpd[1].B=0x54;	cpd[1].A=0xFF;
		cpd[2].R=0xe1;	cpd[2].G=0x60;	cpd[2].B=0x3d;	cpd[2].A=0xFF;
		cpd[3].R=0xe4;	cpd[3].G=0xb7;	cpd[3].B=0x4a;	cpd[3].A=0xFF;
		cpd[4].R=0xa1;	cpd[4].G=0xfc;	cpd[4].B=0x58;	cpd[4].A=0xFF;
		cpd[5].R=0x96;	cpd[5].G=0x54;	cpd[5].B=0xa9;	cpd[5].A=0xFF;
		cpd[6].R=0x00;	cpd[6].G=0x82;	cpd[6].B=0x94;	cpd[6].A=0xFF;
		return cpd[time.tm_wday];

	}
	return colourPerDayArrayOfSeven[time.tm_wday];
}

COLOUR HourToRgb(long ts, COLOUR *cMidnight, COLOUR *cNoon)
{
	COLOUR defaultMidnight;
	COLOUR defaultNoon;

	COLOUR mixedColour;

	struct tm time;
	localtime_s(&ts, &time);

	int hour;

	defaultMidnight.R = 0x08;	defaultMidnight.G = 0x00;	defaultMidnight.B = 0x42;defaultMidnight.A = 0xFF;
	defaultNoon.R = 0x90;	defaultNoon.G = 0xDC;	defaultNoon.B = 0xFF;defaultNoon.A = 0xFF;

	if (cMidnight == NULL)
		cMidnight = &defaultMidnight;
	if (cNoon == NULL)
		cNoon = &defaultNoon;

	hour = abs(time.tm_hour-12);
	//returns absolute value of -12 to +11
	mixedColour.R = (cNoon->R * (12-hour) + cMidnight->R * (hour))/12;
	mixedColour.G = (cNoon->G * (12-hour) + cMidnight->G * (hour))/12;
	mixedColour.B = (cNoon->B * (12-hour) + cMidnight->B * (hour))/12;
	mixedColour.A = (cNoon->A * (12-hour) + cMidnight->A * (hour))/12;


	return mixedColour;
}


int DrawGrid(BM* bm)
{
	//int i;
	int lat, lon;
	double x1,y1;
	double x2,y2;

	int spacing;
	COLOUR c;
	spacing=bm->options->gridsize;
	c=bm->options->gridcolour;

	if ((spacing==0) || (spacing>180))
		return 0;

	for (lat=-90+spacing; lat<90; lat+=spacing)	{
		LatLongToXY(bm, lat, -180, &x1, &y1);
		LatLongToXY(bm, lat, 180, &x2, &y2);
//		printf("%f %f; %f %f\r\n",x1,y1,x2,y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2,1, &c);
	}

	for (lon=-180+spacing; lon<180; lon+=spacing)	{
		LatLongToXY(bm, -90, lon, &x1, &y1);
		LatLongToXY(bm, 90, lon, &x2, &y2);
//		printf("%f %f; %f %f\r\n",x1,y1,x2,y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2,1, &c);
	}


		//Equator
		lat=0;
		LatLongToXY(bm, lat, -180, &x1, &y1);
		LatLongToXY(bm, lat, 180, &x2, &y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2,1, &c);
		c.A=c.A*0.8;
		bitmapLineDrawWu(bm, x1,y1-1,x2,y2-1,1, &c);
		bitmapLineDrawWu(bm, x1,y1+1,x2,y2+1,1, &c);

	return 0;
}


int ColourWheel(BM* bm, int x, int y, int r, int steps)
{
	int i;
	COLOUR c;
	if (steps<1) return 1;
 	for (i=0;i<360;i+=steps) {
 		c=HsvToRgb(i*255/360,255,255,255);
	 	bitmapLineDrawWu(bm, x,y,x+r*sin(i*PI/180),y+r*cos(i*PI/180),1,&c);
 	}

	return 0;
}


int WriteKMLFile(BM* bm)
{
	FILE *kml;
	char sStartTime[80];
	char sEndTime[80];

	char *pngnameNoDirectory;

	pngnameNoDirectory = strrchr(bm->options->pngfilenamefinal, '\\')+1;
	if (!pngnameNoDirectory)
		pngnameNoDirectory=bm->options->pngfilenamefinal;

	strftime (sStartTime,80,"%B %Y",localtime(&bm->options->fromtimestamp));
	strftime (sEndTime,80,"%B %Y",localtime(&bm->options->totimestamp));

	kml=fopen(bm->options->kmlfilenamefinal,"w");
	fprintf(kml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	fprintf(kml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\r\n");
	fprintf(kml, "<GroundOverlay>\r\n");
	fprintf(kml, "<name>WorldTracker - %s</name>\r\n",bm->options->title);
	fprintf(kml, "<description>");
	fprintf(kml, "From %s to %s.\r\n", sStartTime, sEndTime);
	fprintf(kml, "Plotted %i points.\r\n", bm->countPoints);
	fprintf(kml, "</description>\r\n");
	fprintf(kml, "<Icon><href>%s</href></Icon>\r\n",pngnameNoDirectory);
    fprintf(kml, "<LatLonBox>\r\n");
	fprintf(kml, "<north>%f</north>\r\n",bm->options->nswe.north);
	fprintf(kml, "<south>%f</south>\r\n",bm->options->nswe.south);
	fprintf(kml, "<east>%f</east>\r\n",bm->options->nswe.east);
	fprintf(kml, "<west>%f</west>\r\n",bm->options->nswe.west);
	fprintf(kml, "<rotation>0</rotation>\r\n");
	fprintf(kml, "</LatLonBox>\r\n");
	fprintf(kml, "</GroundOverlay>\r\n");
	fprintf(kml, "</kml>\r\n");

	fclose(kml);

	return 1;
}


int LoadPreset(OPTIONS *options, char *preset)
{
	#define PRESET_COUNT 53
	#define MAX_PRESET_LENGTH 32

	char preset_name[PRESET_COUNT][MAX_PRESET_LENGTH];
	double preset_north[PRESET_COUNT];
	double preset_south[PRESET_COUNT];
	double preset_west[PRESET_COUNT];
	double preset_east[PRESET_COUNT];
	int i;

	sprintf(options->title, "%s", preset);
	fprintf(stdout, "Preset: %s\r\n", options->title);

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
strcpy(preset_name[46],"oceania"); preset_north[46]= 35;	preset_south[46]= -50;	preset_west[46]=-220; preset_east[46]=-110;
strcpy(preset_name[47],"fiji"); preset_north[47]= -15.5;	preset_south[47]= -21.14;	preset_west[47]=176.77; preset_east[47]=182.08;
strcpy(preset_name[48],"montreal"); preset_north[48]=  45.711;	preset_south[48]=  45.373;	preset_west[48]=-73.99; preset_east[48]=-73.374;
strcpy(preset_name[49],"southeastasia"); preset_north[49]= 29;	preset_south[49]= -11;	preset_west[49]=91; preset_east[49]=128;
strcpy(preset_name[50],"th"); preset_north[50]= 20.5;	preset_south[50]= 5.5;	preset_west[50]=97; preset_east[50]=106;
strcpy(preset_name[51],"florida"); preset_north[51]= 31;	preset_south[51]= 24.4;	preset_west[51]=-87.65; preset_east[51]=-80;
strcpy(preset_name[52],"world"); preset_north[52]= 90;	preset_south[52]= -90;	preset_west[52]=-180; preset_east[52]=180;

	for (i=0;i<PRESET_COUNT;i++)	{
		if (!stricmp(preset,preset_name[i]))	{
			options->nswe.north=preset_north[i];
			options->nswe.south=preset_south[i];
			options->nswe.west=preset_west[i];
			options->nswe.east=preset_east[i];
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

double MetersApartFlatEarth(double lat1, double long1, double lat2, double long2)	//let's say 1=origin, 2=destination
{
	//convert to radians
	lat1*=PI/180;
	long1*=PI/180;
	lat2*=PI/180;
	long2*=PI/180;

	double deltalat=lat2-lat1;
	double deltalong = long2-long1;
	double meanlat=(lat1+lat2)/2;
	//double a = PI/2 - lat1;
	//double b = PI/2 - lat2;
	//double u = a * a + b * b;
//		printf("%f ",deltalong);

	//double v = -2 * a * b * cos(long2 - long1);
	//double c = sqrt(abs(u + v));

	double c=sqrt( (deltalat*deltalat) + ((cos(meanlat)*deltalong)*(cos(meanlat)*deltalong)) );
	return EARTH_MEAN_RADIUS_KM*1000 * c;
}

int CopyNSWE(NSWE *dest, NSWE *src)
{
	//this just does n=n, e=e, etc.
	memcpy(dest, src, sizeof(*dest));
	return 0;
}


TRIP * GetLinkedListOfTrips(NSWE * a, NSWE * b, LOCATIONHISTORY *lh)
{
	LOCATION *loc;
	WORLDCOORD coord;

	int dir;

	TRIP * trip;
	TRIP * oldtrip;
	TRIP *firsttrip;

	long leavetime;

	leavetime=0;
	trip=NULL;
	oldtrip=NULL;
	firsttrip=NULL;

	dir = 0;
	loc=lh->first;
	while (loc)	{
		coord.latitude = loc->latitude;
		coord.longitude = loc->longitude;

		if (CoordInNSWE(&coord, a))	{
			if (dir<1)	{
				trip = malloc(sizeof(TRIP));
				trip->next=NULL;
				if (!firsttrip)	firsttrip=trip;
				if (oldtrip)	oldtrip->next=trip;
				trip->arrivetime = loc->timestampS;
				trip->leavetime = leavetime;
				trip->direction=dir;
				oldtrip=trip;
				dir =1;
			}
			else	{
				leavetime=loc->timestampS;
			}
		}

		else if (CoordInNSWE(&coord, b))	{
			if (dir>-1)	{
				trip = malloc(sizeof(TRIP));
				trip->next=NULL;
				if (!firsttrip)	firsttrip=trip;
				if (oldtrip)	oldtrip->next=trip;
				trip->arrivetime = loc->timestampS;
				trip->leavetime = leavetime;
				trip->direction=dir;
				oldtrip=trip;
				dir =-1;
			}
			else	{
				leavetime=loc->timestampS;
			}
		}


		loc=loc->next;
	}

	return firsttrip;
}

int ExportTripData(TRIP * trip, char * filename)
{
	FILE *f;
	struct tm leavetime;
	struct tm arrivetime;

	f=fopen(filename, "w");
	if (!f) return 0;
	fprintf(f, "leavetime,arrivetime,seconds, direction,year,month,dayofmonth,dayofweek,hour,minute\r\n");
	while (trip)	{
		if (trip->direction!=0)	{
			localtime_s(&trip->leavetime, &leavetime);
			localtime_s(&trip->arrivetime, &arrivetime);

			fprintf(f, "%i,%i,%i,%i, %i,%i,%i,%i,%i,%i\r\n", trip->leavetime, trip->arrivetime, trip->leavetime- trip->arrivetime, trip->direction,
				leavetime.tm_year,leavetime.tm_mon,leavetime.tm_mday,leavetime.tm_wday,leavetime.tm_hour,leavetime.tm_min);
		}
		trip=trip->next;
	}
	fclose(f);

	return 0;

}

int FreeLinkedListOfTrips(TRIP * trip)
{
	TRIP * remembertrip;
	while (trip)	{
		remembertrip = trip;
		free(trip);
		trip=remembertrip->next;
	}
	return 0;
}

int CoordInNSWE(WORLDCOORD *coord, NSWE *nswe)	//is a coord within a nswe region?
{
	if (coord->latitude > nswe->north)
		return 0;
	if (coord->latitude < nswe->south)
		return 0;
	if (coord->longitude < nswe->west)
		return 0;
	if (coord->longitude > nswe->east)
		return 0;


	return 1;
}
