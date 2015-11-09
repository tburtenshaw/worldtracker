#define VERSION 0.37

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


				case 'z':
				case 'Z':
					if (i+1<argc)	{
						options->zoom = strtod(argv[i+1], NULL);
						i++;	//move to the next variable, we've got a zoom
					}
					break;
				case 'g':	//the grid
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
