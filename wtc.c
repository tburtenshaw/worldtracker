#define VERSION 0.20160118

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "mytrips.h"
#include "wtc.h"
#include "lodepng.h"

int main(int argc,char *argv[])
{
	BM	mainBM;
	OPTIONS options;
	LOCATIONHISTORY locationHistory;

	int error;


	//set vars to zero
	memset(&options, 0, sizeof(options));	//set all the option results to 0
	memset(&mainBM, 0, sizeof(mainBM));

	//Display introductory text
	PrintIntro(argv[0]);

	//locationHistory.jsonfilename="LocationHistory.json";	//default

	if (argc == 1)	PrintUsage(argv[0]);	//print usage instructions
	HandleCLIOptions(argc, argv, &options);
	PrintOptions(&options);
	RationaliseOptions(&options);

	//Initialise the bitmap
	bitmapInit(&mainBM, &options, &locationHistory);
	DrawGrid(&mainBM);
	//ColourWheel(mainBM, 100, 100, 100, 5);  	//Color wheel test of lines and antialiasing


	fprintf(stdout, "Loading locations from %s.\r\n", mainBM.options->jsonfilenamefinal);
	LoadLocations(&locationHistory, &mainBM.options->jsonfilenamefinal[0]);

	fprintf(stdout, "Plotting paths.\r\n");
	PlotPaths(&mainBM, &locationHistory, &options);
//	HeatmapPlot(&mainBM, &locationHistory);

	fprintf(stdout, "Ploted lines between: %i points\r\n", mainBM.countPoints);
	printf("From:\t%s\r\n", asctime(localtime(&locationHistory.earliesttimestamp)));
	printf("To:\t%s\r\n", asctime(localtime(&locationHistory.latesttimestamp)));

	FreeLocations(&locationHistory);


	//Write the PNG file
	fprintf(stdout, "Writing to %s.\r\n", mainBM.options->pngfilenamefinal);
	error = lodepng_encode32_file(mainBM.options->pngfilenamefinal, mainBM.bitmap, mainBM.width, mainBM.height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));

	//Write KML file
	fprintf(stdout, "Writing to %s.\r\n", mainBM.options->kmlfilenamefinal);
	WriteKMLFile(&mainBM);

	bitmapDestroy(&mainBM);
	fprintf(stdout, "Program finished.");
	return 0;
}

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


int HandleCLIOptions(int argc,char *argv[], OPTIONS *options)
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

/* //I've stopped using zoom as an argument
				case 'z':
				case 'Z':
					if (i+1<argc)	{
						options->zoom = strtod(argv[i+1], NULL);
						i++;	//move to the next variable, we've got a zoom
					}
					break;
					*/
				case 'g':	//the grid
				case 'G':
					if (i+1<argc)	{
						options->gridsize = strtod(argv[i+1], NULL);
						i++;
					}
					break;
				case 'c':	//how to cycle through the colours
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
				case 'a':	//alpha value
				case 'A':
					if (i+1<argc)	{
						options->alpha = strtod(argv[i+1], NULL);
						i++;
					}
					break;
				case 'f':	//from time
				case 'F':
					if (i+1<argc)	{
						options->fromtimestamp = strtol(argv[i+1], NULL,0);
						i++;
					}
					break;
				case 't':	//to time
				case 'T':
					if (i+1<argc)	{
						options->totimestamp = strtol(argv[i+1], NULL,0);
						i++;
					}
					break;
				case 'k':
				case 'K':
					if (i+1<argc)	{
						options->kmlfilenameinput = argv[i+1];
						i++;
					}
					break;
				case 'i':
				case 'I':
					if (i+1<argc)	{
						options->jsonfilenameinput = argv[i+1];
						i++;
					}
					break;
				case 'o':
				case 'O':
					if (i+1<argc)	{
						options->pngfilenameinput = argv[i+1];
						i++;
					}
					break;
				default:
					fprintf(stderr,"unknown option %s\n",argv[i]);
					break;
			}
		}
		else {
			options->pngfilenameinput = argv[i];
			firstnonoption = i;
			break;
		}
	}
	return firstnonoption;
}

void PrintOptions(OPTIONS *options)
{

	//print some of the options
	fprintf(stdout, "From the options:\r\n");
	fprintf(stdout, "Input file: %s\r\n", options->jsonfilenameinput);	//this won't work, as it's a pointer to somewhere in argv
	fprintf(stdout, "N %f, S %f, W %f, E %f\r\n",options->north,options->south,options->west,options->east);
	fprintf(stdout, "KML: %s\r\n",options->kmlfilenameinput);
	fprintf(stdout, "Title: %s", options->title);
	fprintf(stdout, "\r\n");
	return;
}

int RationaliseOptions(OPTIONS *options)
{

	double tempdouble;
	int testwidth;
	char noext[MAX_PATH];
	char *period;


	if (options->colourcycle==0) options->colourcycle = (60*60*24);//(60*60*24*183);

	//Load appropriate filenames
/*
	if ((options->pngfilenameinput==NULL) && (options->kmlfilenameinput))	{	//if there's a kml, no png named
		MakeProperFilename(options->kmlfilenamefinal, options->kmlfilenameinput, "trips.kml", "kml");

		strcpy(&noext[0], options->kmlfilenamefinal);	//copy the finalised png filename
		period=strrchr(noext,'.');			//find the final period
		*period=0;								//replace it with a null to end it

		sprintf(options->pngfilenameinput, "%s.png", noext);
	}
*/
	MakeProperFilename(options->pngfilenamefinal, options->pngfilenameinput, "trips.png", "png");


	if (options->kmlfilenameinput==NULL)	{
		strcpy(&noext[0], options->pngfilenamefinal);	//copy the finalised png filename
		period=strrchr(noext,'.');			//find the final period
		*period=0;								//replace it with a null to end it
		fprintf(stdout, "KML no ext: %s\r\n",noext);
		MakeProperFilename(options->kmlfilenamefinal, noext, "fallback.kml", "kml");
	} else
	{
		MakeProperFilename(options->kmlfilenamefinal, options->kmlfilenameinput, "fallback.kml", "kml");
	}

	MakeProperFilename(options->jsonfilenamefinal, options->jsonfilenameinput, "LocationHistory.json", "json");
	fprintf(stdout, "JSON: %s\r\n",options->jsonfilenamefinal);
	fprintf(stdout, "KML: %s\r\n",options->kmlfilenamefinal);

	//Set the zoom
//	if (((options->zoom<0.1) || (options->zoom>55)) && (options->zoom!=0))	{	//have to decide the limit, i've tested to 55
//		if (options->zoom) fprintf(stderr, "Zoom must be between 0.1 and 55\r\n");	//if they've entered something silly
//		options->zoom = 0;	//set to zero, then we'll calculate.
//	}

	//if they're the wrong way around
	if (options->nswe.east<options->nswe.west)	{tempdouble=options->nswe.east; options->nswe.east=options->nswe.west; options->nswe.west=tempdouble;}
	if (options->nswe.north<options->nswe.south)	{tempdouble=options->nswe.north; options->nswe.north=options->nswe.south; options->nswe.south=tempdouble;}

	//if they're strange
	if ((options->nswe.east-options->nswe.west == 0) || (options->nswe.north-options->nswe.south==0) || (options->nswe.east-options->nswe.west>360) || (options->nswe.north-options->nswe.south>360))	{
		options->nswe.west=-180;
		options->nswe.east=180;
		options->nswe.north=90;
		options->nswe.south=-90;
	}

/*
	if ((options->height==0) && (options->width == 0))	{	//if no height or width specified, we'll base it on zoom (or default zoom)
		if (options->zoom==0)	{
			options->zoom=10;
		}	//default zoom
		options->width=360*options->zoom;
		options->height= 180*options->zoom;
	}	else
	if (options->width==0)	{	//the haven't specified a width (but have a height)
		options->width=options->height*(options->nswe.east-options->nswe.west)/(options->nswe.north-options->nswe.south);
		if (options->width > MAX_DIMENSION)	{	//if we're oversizing it
			options->width = MAX_DIMENSION;
			options->height = options->width*(options->nswe.north - options->nswe.south)/(options->nswe.east - options->nswe.west);
		}
	}	else
	if (options->height==0)	{	//the haven't specified a height (but have a width)
			options->height=options->width*(options->nswe.north-options->nswe.south)/(options->nswe.east-options->nswe.west);
			if (options->height > MAX_DIMENSION)	{	//if we're oversizing it
				options->height = MAX_DIMENSION;
				options->width=options->height*(options->nswe.east-options->nswe.west)/(options->nswe.north-options->nswe.south);
			}
	}
*/

	//test for strange rounding errors
	if ((options->width==0) || (options->height==0) || (options->height > MAX_DIMENSION) || (options->width > MAX_DIMENSION))	{
		fprintf(stderr, "Problem with dimensions (%i x %i). Loading small default size.\r\n", options->width, options->height);
		options->nswe.west=-180;
		options->nswe.east=180;
		options->nswe.north=90;
		options->nswe.south=-90;
		options->height=180;
		options->width=360;
	}

	//if the aspect ratio of coords is different, set the width to be related to the
	testwidth=options->height*(options->nswe.east-options->nswe.west)/(options->nswe.north-options->nswe.south);
	if (testwidth != options->width)	{
		printf("Fixing aspect ratio. tw: %i, w: %i\r\n", testwidth, options->width);
		options->width=testwidth;
	}

	//then calculate how many pixels per degree
	//options->zoom=options->width/(options->nswe.east-options->nswe.west);
	//fprintf(stdout, "Zoom: %4.2f\r\n", options->zoom);

	//Set the from and to times
	if (options->totimestamp == 0)
		options->totimestamp =-1;

	//Set the thickness
	if (options->thickness == 0)
		options->thickness=1;

	//options->thickness = 1+options->width/1000;

	//Set the alpha of the line
	options->alpha=200;	//default


	//Grid colour
	options->gridcolour.R=192;options->gridcolour.G=192;options->gridcolour.B=192;options->gridcolour.A=128;

	options->colourby = COLOUR_BY_TIME;
	options->forceheight = 0;

	return 0;
}


