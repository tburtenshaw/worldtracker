#define VERSION 0.33

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "mytrips.h"
#include "wtc.h"

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

