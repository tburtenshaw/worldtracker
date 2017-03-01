#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "lodepng.h"
#include "mytrips.h"
#include "rasterfont.h"

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

	bm->zoom = bm->width/(bm->nswe.east-bm->nswe.west);

for (int g=0; g<locationHistory->numgroups; g++)	{
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
		else if (options->colourby == COLOUR_BY_GROUP)	{
			if (g==0)	{c.R=255; c.G=200; c.B =255; c.A=120;}
			if (g==1)	{c.R=200; c.G=255; c.B =255; c.A=120;}
			if (g==2)	{c.R=255; c.G=255; c.B =200; c.A=120;}
			if (g==3)	{c.R=255; c.G=200; c.B =200; c.A=120;}
			if (g==4)	{c.R=200; c.G=255; c.B =200; c.A=120;}
			if (g==5)	{c.R=200; c.G=200; c.B =255; c.A=120;}
		}



		if ((coord->timestampS >= options->fromtimestamp) && (coord->timestampS <= options->totimestamp) && (coord->group == g))	{
			//draw a line from last point to the current one.
			bm->countPoints+= bitmapCoordLine(bm, coord->latitude, coord->longitude, oldlat, oldlon, options->thickness, &c);
			oldlat=coord->latitude;oldlon=coord->longitude;
		}

		//Move onto the next appropriate coord
		if (bm->zoom>10000)	coord=coord->next;
		else if (bm->zoom>1000) coord=coord->next10000ppd;
		else if (bm->zoom>100) coord=coord->next1000ppd;
		else if (bm->zoom>10) coord=coord->next100ppd;
		else if (bm->zoom>1) coord=coord->next10ppd;
		else coord=coord->next1ppd;

	}

}


	RASTERFONT rf;
	LoadRasterFont(&rf);
//	bitmapText(bm, &rf,10,10,"5:45pm 1.2.3.4.5.6.7.8.9.0 $4.50 for 6! 1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ", &c);
//	plotChar(bm, &rf,0,0,65, &c);

	DestroyRasterFont(&rf);
	return 0;
}


int LoadLocations(LOCATIONHISTORY *locationHistory, char *jsonfilename, void(*progressfn)(int))
{
	LOCATION *coord;
	LOCATION *prevCoord;

	LOCATION *firstCoord;
	LOCATION *lastCoord;

	int inputfiletype;


	//Open the input file
	locationHistory->json=fopen(jsonfilename,"r");
	if (locationHistory->json==NULL)	{
		fprintf(stderr, "\nUnable to open \'%s\'.\n", jsonfilename);
		perror("Error");
		fprintf(stderr, "\n");
		return 1;
	}


	//Now read it and load it into a linked list
	coord=calloc(sizeof(LOCATION),1);

	//this will load into its own separate linked list initially, before being sorted then combined
	firstCoord = coord;
	prevCoord=NULL;

	int progress=0;
	long twofiftysixth;

	//get the file size, then reset the position
	fseek(locationHistory->json, 0, SEEK_END);
	locationHistory->filesize = ftell(locationHistory->json);
	rewind(locationHistory->json);
	locationHistory->filepos=0;

	twofiftysixth = locationHistory->filesize/256;

	inputfiletype = GuessInputFileType(locationHistory);

	while (ReadLocation(locationHistory, coord, inputfiletype))	{
		locationHistory->numPoints++;
		if (progressfn)	{
			if (locationHistory->filepos > twofiftysixth * progress)	{
				progress++;
				(*progressfn)(progress);
			}
		}


		coord->group=locationHistory->numgroups;	//set the group number
		coord->inputfileindex = locationHistory->numinputfiles;

		coord->prev=prevCoord;
		coord->next=calloc(sizeof(LOCATION),1);	//allocate memory for the next in the linked list

		prevCoord=coord;

		//Advance to the next in the list
		coord= coord->next;
	}
	free(coord);//remove the last allocated
	fclose(locationHistory->json);

	if (prevCoord)	{
		prevCoord->next=NULL;	//close off the linked list
		lastCoord=prevCoord;
	}


	printf("\nSorting loaded list...");
	int n=0;
	printf("\nbefore sort m:%i ets %i lts %i", firstCoord, firstCoord->timestampS, lastCoord->timestampS);
   	n = SortLocationsInsertSort(&firstCoord, &lastCoord);
	printf("\nafter sort m:%i ets %i lts %i", firstCoord, firstCoord->timestampS, lastCoord->timestampS);



	if (!locationHistory->first)	{	//if there is no data yet, then the first and last of loaded coords will be the first and last of all
		locationHistory->first = firstCoord;
		locationHistory->last = lastCoord;
	}
	else	{	//if we already have loaded something
//		locationHistory->last->next = firstCoord;
//		firstCoord->prev=locationHistory->last;
		printf("\nMerging...");
		MergeLocationGroup(&locationHistory->first, &locationHistory->last, &firstCoord, &lastCoord);
	}


	//Adds it to a linked list of open files
	IMPORTEDFILE tempImportedFile;	//temp which is copied to a new malloc'd one
	tempImportedFile.id = locationHistory->numinputfiles;
	tempImportedFile.group = locationHistory->numgroups;
	tempImportedFile.filesize = locationHistory->filesize;
	tempImportedFile.filetype = inputfiletype;
	strcpy(tempImportedFile.fullFilename, jsonfilename);	//need to limit to MAX_PATH

	printf("\naif%i", AddInputFile(locationHistory, &tempImportedFile));


	//printf("\nSorting whole list");
	//n=SortLocationsInsertSort(&locationHistory->first, &locationHistory->last);



	printf("\nOptimising...");
	OptimiseLocations(locationHistory);

	return 0;
}

LOCATION * DeleteLocation(LOCATIONHISTORY *lh, LOCATION *locToDelete)	//returns the "next" location
{
	LOCATION * prev;
	LOCATION * next;

	next = locToDelete->next;
	prev = locToDelete->prev;

	if (next && prev)	{	//there's both a previous and next, so we're in the middle of the linked link
		locToDelete->prev->next = next;
		locToDelete->next->prev = prev;
//		printf("deleting %i",locToDelete);
		free(locToDelete);
		return next;
	}
	else if	(next)	{	//there's a next, but no prev, so must be first
		locToDelete->next->prev = NULL;	//prev will be NULL;
		lh->first =next;
		free(locToDelete);
		return next;
	}
	else if (prev)	{	//there's a prev, no next, so we're at the end of the list (probably don't need this "else if" statement)
		locToDelete->prev->next = NULL;
		lh->last = prev;
		free(locToDelete);
		return NULL;
	}
	return next;
}

int	AddInputFile(LOCATIONHISTORY *lh, IMPORTEDFILE *importedFile)
{
	IMPORTEDFILE *current;
	IMPORTEDFILE *newFile;


	lh->numinputfiles++;
	lh->numgroups++;


	newFile = malloc(sizeof(IMPORTEDFILE));
	memcpy(newFile, importedFile, sizeof(IMPORTEDFILE));
	newFile->next = NULL;


	if (lh->firstImportedFile == NULL)	{
		lh->firstImportedFile = newFile;
		return 1;
	}

	current= lh->firstImportedFile;

	//traverse to the end of the list
	while (current->next)	{
		current=current->next;
	}
	current->next = newFile;

	return 0;
}

IMPORTEDFILE * GetInputFileByIndex(LOCATIONHISTORY *lh, int id)
{
	IMPORTEDFILE *current;
	int i;

	current = lh->firstImportedFile;
	i=0;

	while (current)	{
		if (i == id)	return current;
		current = current->next;
		i++;
	}

	return	NULL;
}

int DeleteInputFile(LOCATIONHISTORY *lh, int id)
{
	IMPORTEDFILE *current;
	IMPORTEDFILE *next;
	IMPORTEDFILE *prev;

	LOCATION *loc;

	//print the list
	/*
	current = lh->firstImportedFile;
	while (current)	{
		printf("\n%i %s", current->id, current->fullFilename);
		current=current->next;
	}
*/


	//Remove the file from the importedfile linked list
	current = lh->firstImportedFile;
	while (current->id < id)	{
		prev = current;
		next = current->next;
		current = current->next;
	}
	if (current->id == id)	{
		next = current->next;
		printf("\ndeleting %i %s", current->id, current->fullFilename);
		free(current);

		if (id >0)	{
			prev->next =next;
		}
		else if (id ==0)	{
			lh->firstImportedFile = next;
		}
	}

	while (next)	{
		next->id--;
		next=next->next;
	}

	//then remove all the associated coordinates
	loc = lh->first;

	while (loc)	{
		if (loc->inputfileindex == id)	{
			//delete the point
			loc = DeleteLocation(lh, loc);	//this moves to the next
		}

		else if (loc->inputfileindex>id)	{	//shift anything higher down 1, then move on
			loc->inputfileindex--;
			loc = loc->next;
		}
		else	{	//move on without doing anything
			loc = loc->next;
		}


	}


	//print the list
/*
	current = lh->firstImportedFile;
	while (current)	{
		printf("\n%i %s", current->id, current->fullFilename);
		current=current->next;
	}
*/
	lh->numinputfiles--;
	OptimiseLocations(lh);	//must do this, otherwise the skipping part can miss things
	return 0;
}


//This merges two sorted groups into one (New into Base)
int MergeLocationGroup(LOCATION **ppFirstBase, LOCATION **ppLastBase, LOCATION **ppFirstNew, LOCATION **ppLastNew)
{
	LOCATION *locBase;
	LOCATION *locNew;

	LOCATION *next;

	locBase=*ppFirstBase;
	locNew=*ppFirstNew;

	while ((locNew) && (locBase))	{
		if (locNew->timestampS < locBase->timestampS)	{
		//	printf("*");
			next = locNew->next;
			RemoveLocationFromList(ppFirstNew, ppLastNew, locNew);
			InsertLocationBefore(ppFirstBase, locNew, locBase);
			locNew = next;
		}
		else	{
			if ((locBase->next == NULL)&&(locNew))	{	//if we're at the end of the base list, at the remaining new list to it
		//		printf("adding to end");
				locBase->next = locNew;
				locNew->prev = locBase;
				*ppLastBase = *ppLastNew;
				locBase = *ppLastNew;
			}
			locBase = locBase->next;
		}
	}
	//Need to make the group the same

	return 0;
}

int SortLocationsInsertSort(LOCATION **ppFirst, LOCATION **ppLast)
{
	LOCATION *loc;
	LOCATION *place;
	LOCATION *chosen;

//	LOCATION *beginsearch;

	loc=*ppFirst;
	//beginsearch  =*ppFirst;
	if (!loc) return 0;

	int n=0;
	int s=0;
	long i=0;
	while (loc->next)	{
		n++;
		//printf("\n%i", loc->timestampS);
		if (loc->timestampS > loc->next->timestampS)	{		//if the timestamp is later than the ->next element, there is disorder
//			printf("\n%i > %i, at %i > %i",loc->timestampS, loc->next->timestampS, loc, loc->next);
			s++;
			chosen=loc->next;
			place=*ppFirst;						//set at the start
//			printf("\nplace %i", place);

//			printf("\nplace ts %i chosen ts:%i, loc %i",place->timestampS, chosen->timestampS, loc);
			while (place->timestampS < chosen->timestampS)	{	//then find where to insert the next element
				i++;
				place=place->next;
			}
//			printf("\nplace %i, first %i", place, *ppFirst);
//			printf("\nRemoving %i", chosen);
			RemoveLocationFromList(ppFirst, ppLast, chosen);	//remove the location from its current position
//			printf(", and inserting %i to before %i", chosen, place);
//			printf("\nplace %i, first %i", place, *ppFirst);
			InsertLocationBefore(ppFirst, chosen, place);	//then insert it after the next pos
//			printf(" without crashing.");
//			printf("\nplace %i, first %i", place, *ppFirst);
		}
		else	{	//move onto the next one, otherwise we stay the same, but check the next
			loc = loc->next;
		}
	}


//	printf("\nlast %i %i", loc, locationHistory->last);
	printf("\n%i iterated, %i moved", i, s);
	return n;
}

void RemoveLocationFromList(LOCATION **ppFirst, LOCATION **ppLast, LOCATION *loc)
{
	//fix the previous one to skip over
	if (loc->prev)	{	//if it's not the first
		loc->prev->next = loc->next;
	}
	else	{
		*ppFirst = loc->next;
	}


	//then the next one
	if (loc->next)	{	//if it's not the last item
		loc->next->prev = loc->prev;
	}
	else	{
		*ppLast = loc->prev;
	}

	return;
}

void InsertLocationBefore(LOCATION **ppFirst, LOCATION *loc, LOCATION *target)
{
//	printf("\n ILB: First %i", *ppFirst);
	loc->next=target;
	loc->prev=target->prev;

	if (target->prev)	{
//		printf("\nInserting later...");
		target->prev->next = loc;
	}
	else	{
//		printf("\nInserting %i to first (*%i, %i)...", loc, *ppFirst, ppFirst);
		*ppFirst = loc;
//		printf(" done.");
	}

	target->prev=loc;
//	printf("\ntp=l");
	return;

}



int OptimiseLocations(LOCATIONHISTORY *locationHistory)	//even though this is called optimise, it must be called after deleting points etc.
{

	LOCATION *loc;

	LOCATION *waitingFor1;
	LOCATION *waitingFor10;
	LOCATION *waitingFor100;
	LOCATION *waitingFor1000;
	LOCATION *waitingFor10000;

	LOCATION *prevLoc;


	NSWE closestbounds;
	unsigned long earlyts;
	unsigned long latets;

	//set these as beatable
	closestbounds.north=-90;
	closestbounds.south=90;
	closestbounds.west=180;
	closestbounds.east=-180;
	earlyts = -1;	//the largest unsigned number
	latets=0;

	//This function allows the plotter to skip to the next significant line at a given resolution (pixels per degree)
	loc=locationHistory->first;

	waitingFor1 = loc;	//these are the points waiting to see for the next one at a given resolution
	waitingFor10 = loc;
	waitingFor100 = loc;
	waitingFor1000 = loc;
	waitingFor10000 = loc;

	//It also calculates speed and time difference and distance between points
	prevLoc = NULL;

	while (loc)	{
		//the "waiting fors" are a chain, only calculated for the links in the chain
		//so if I accidentally went to a ->waitingFor10 in something that it wasn't calculated for, it would potentially crash
		//this makes the default move onto the next, until it finds one with a defined next point
		loc->next1ppd = loc->next;
		loc->next10ppd = loc->next;
		loc->next100ppd = loc->next;
		loc->next1000ppd = loc->next;
		loc->next10000ppd = loc->next;



		//set the next of the previous depending on our zoom resolution
		if ((fabs(waitingFor1->latitude - loc->latitude) >1) ||(fabs(waitingFor1->longitude - loc->longitude) >1))	{
			waitingFor1->next1ppd = loc;
			waitingFor1=loc;
		}

		if ((fabs(waitingFor10->latitude - loc->latitude) >0.1) ||(fabs(waitingFor10->longitude - loc->longitude) >0.1))	{
			waitingFor10->next10ppd = loc;
			waitingFor10=loc;
		}

		if ((fabs(waitingFor100->latitude - loc->latitude) >0.01) ||(fabs(waitingFor100->longitude - loc->longitude) >0.01))	{
			waitingFor100->next100ppd = loc;
			waitingFor100=loc;
		}

		if ((fabs(waitingFor1000->latitude - loc->latitude) >0.001) ||(fabs(waitingFor1000->longitude - loc->longitude) >0.001))	{
			waitingFor1000->next1000ppd = loc;
			waitingFor1000=loc;
		}

		if ((fabs(waitingFor10000->latitude - loc->latitude) >0.0001) ||(fabs(waitingFor10000->longitude - loc->longitude) >0.0001))	{
			waitingFor10000->next10000ppd = loc;
			waitingFor10000=loc;
		}


		//bounds of earliest, latest, west, east
		if (loc->timestampS > latets)	{
			latets = loc->timestampS;
		}

		if (loc->timestampS < earlyts)	{
			earlyts = loc->timestampS;
			printf("\nEarly %i", earlyts);
		}

		if (loc->latitude > closestbounds.north)	{
			closestbounds.north = loc->latitude;
		}

		if (loc->latitude < closestbounds.south)	{
			closestbounds.south = loc->latitude;
		}

		if (loc->longitude < closestbounds.west)	{
			closestbounds.west = loc->longitude;
		}

		if (loc->longitude > closestbounds.east)	{
			closestbounds.east = loc->longitude;
		}



		//Distance, speed, deltaT calculations here (Might just have distance, and time change, then calculate speed when required).
		if (prevLoc)	{
			loc->distancefromprev = MetersApartFlatEarth(prevLoc->latitude, prevLoc->longitude, loc->latitude, loc->longitude);
			loc->secondsfromprev = loc->timestampS - prevLoc->timestampS;
//		printf("%fm (%i to %i) %is\t",coord->distancefromprev, prevCoord->timestampS, coord->timestampS, coord->secondsfromprev);
		}
		prevLoc=loc;


		loc=loc->next;
	}

	locationHistory->earliesttimestamp = earlyts;
	locationHistory->latesttimestamp = latets;

	CopyNSWE(&locationHistory->bounds, &closestbounds);

//	locationHistory->earliesttimestamp=locationHistory->first->timestampS;
//	printf("\nets: %i",locationHistory->earliesttimestamp);

//	locationHistory->latesttimestamp=locationHistory->last->timestampS;
//	printf("\nlts: %i",locationHistory->latesttimestamp);

	return 0;
}

int CalculateSpeed(LOCATIONHISTORY *locationHistory)
{
	LOCATION *current;
	LOCATION *advance;
	LOCATION *behind;
	LOCATION *comparison;

	current = locationHistory->first;
	while (current)	{
		advance = current->next;
		behind = current->prev;

//		while (DistanceBetween(adva

	}
	return 0;
}


int PrintLocations(LOCATIONHISTORY *locationHistory)	//for debugging
{
	LOCATION *loc;

	int i=0;
	loc = locationHistory->first;
	while (loc)	{
		printf("\n%i->%i",loc, loc->timestampS);
		loc=loc->next;
	}
	return i;
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

int GuessInputFileType(LOCATIONHISTORY *lh)
{
	char buffer[256];

	rewind(lh->json);
	fgets(buffer, 256, lh->json);

	rewind(lh->json);
	if (buffer[0] == '{')	{
		return FT_JSON;
	}
	if (buffer[0] == '@')	{
		return FT_NMEA;
	}
	if (buffer[0] == 'L')	{
		return FT_BACKITUDECSV;
	}


	return FT_UNKNOWN;
}


int ReadLocation(LOCATIONHISTORY *lh, LOCATION *location, int filetype)
{
	switch (filetype)	{
		case FT_NMEA:
			return ReadLocationFromNmea(lh, location);
			break;
		case FT_BACKITUDECSV:
			return ReadLocationFromBackitudeCSV(lh, location);
			break;

		default:
			return ReadLocationFromJson(lh, location);
	}
}

int ReadLocationFromBackitudeCSV(LOCATIONHISTORY *lh, LOCATION *location)
{
	char buffer[256];
	char * pLat;
	char * pLong;
	char * pAcc;
	char * pAlt;
	char * pTSLoc;
	char * pTSUTC;

	struct tm time;


	while (fgets(buffer, 256, lh->json))	{
		lh->filepos+=strlen(buffer);

		if ((buffer[0]<45) || (buffer[0]>57))	{	//if it's not -./012345678 or 9
			if (strncmp("Latitude,Longitude,Accuracy,Altitude,Location Timestamp,Request Timestamp", buffer, 73))	{
				//if it's not identical to that starting string, we don't trust it
				return 0;
			}
		}
		else	{	//we start with a number, minus (or a slash)
			pLat=buffer;
			pLong = strchr(pLat,',');
			*pLong = (char)0; pLong++;

			pAcc = strchr(pLong,',');
			*pAcc = (char)0; pAcc++;

			pAlt = strchr(pAcc,',');
			*pAlt = (char)0; pAlt++;

			pTSLoc = strchr(pAlt,',');
			*pTSLoc = (char)0; pTSLoc++;

			pTSUTC = strchr(pTSLoc,',');
			*pTSUTC = (char)0; pTSUTC++;

			location->latitude = strtod(pLat, NULL);
			location->longitude = strtod(pLong, NULL);

			time.tm_year  = ((pTSUTC[0]-48)*1000 + (pTSUTC[1]-48)*100 + (pTSUTC[2]-48)*10 + (pTSUTC[3]-48))-1900;
			time.tm_mon = (pTSUTC[5]-48)*10 + (pTSUTC[6]-48) - 1;
			time.tm_mday = (pTSUTC[8]-48)*10 + (pTSUTC[9]-48);


			time.tm_hour = (pTSUTC[11]-48)*10 + (pTSUTC[12]-48);
			time.tm_min  = (pTSUTC[14]-48)*10 + (pTSUTC[15]-48);
			time.tm_sec  = (pTSUTC[17]-48)*10 + (pTSUTC[18]-48);

			location->timestampS =	mktime(&time);



			printf("\n%f %f %i", location->latitude,location->longitude,location->timestampS);
			return 1;


		}
	}

	return 0;
}

int ReadLocationFromNmea(LOCATIONHISTORY *lh, LOCATION *location)
{
	char buffer[256];
	const int cmdGGA=1;
	const int cmdRMC=2;
	int nmeaCmd;

	char * pUTCtime;
	char * pStatus;
	char * pLat;
	char * pNorS;
	char * pLong;
	char * pEorW;
	char * pSpeed;
	char * pTrack;
	char * pDate;
	char * pMagVar;

	struct tm time;

	//there are more, but we won't use these


	while (fgets(buffer, 256, lh->json) != NULL)	{
		lh->filepos+=strlen(buffer);
		if (buffer[0]=='$')	{	//we have a proper line
			if ((buffer[3]=='G') && (buffer[4]=='G') && (buffer[5]=='A'))	{
				nmeaCmd  = cmdGGA;
			}
			else if ((buffer[3]=='R') && (buffer[4]=='M') && (buffer[5]=='C'))	{
				nmeaCmd  = cmdRMC;
			}
			else nmeaCmd = 0;

		//$--RMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,xxxx,x.x,a,m,*hh<CR><LF>
			if (nmeaCmd == cmdRMC)	{
				pUTCtime = strchr(buffer,',');	//find the first comma
				*pUTCtime = (char)0; pUTCtime++;	//change the comma to a /0 (to terminate string), then advance by one to get the start of the next string

				pStatus = strchr(pUTCtime,',');
				*pStatus = (char)0; pStatus++;

				pLat = strchr(pStatus,',');	//note we use the string one before
				*pLat = (char)0; pLat++;

				pNorS = strchr(pLat,',');
				*pNorS = (char)0; pNorS++;

				pLong = strchr(pNorS,',');
				*pLong = (char)0; pLong++;

				pEorW = strchr(pLong,',');
				*pEorW = (char)0; pEorW++;

				pSpeed = strchr(pEorW,',');
				*pSpeed = (char)0; pSpeed++;

				pTrack = strchr(pSpeed,',');
				*pTrack = (char)0; pTrack++;

				pDate = strchr(pTrack,',');
				*pDate = (char)0; pDate++;

				pMagVar = strchr(pDate,',');
				*pMagVar = (char)0; pMagVar++;

				time.tm_hour = (pUTCtime[0]-48)*10 + (pUTCtime[1]-48);
				time.tm_min  = (pUTCtime[2]-48)*10 + (pUTCtime[3]-48);
				time.tm_sec  = (pUTCtime[4]-48)*10 + (pUTCtime[5]-48);

				time.tm_mday  = (pDate[0]-48)*10 + (pDate[1]-48);
				time.tm_mon = (pDate[2]-48)*10 + (pDate[3]-48) - 1;
				time.tm_year  = (pDate[4]-48)*10 + (pDate[5]-48) + 100;

				location->timestampS =	mktime(&time);

				//Format is lat: DDMM.MM (degrees and minutes to 1/100th a minute)
				location->latitude=(pLat[0]-48)*10+(pLat[1]-48) + (((double)pLat[2]-48)*10 + ((double)pLat[3]-48) + ((double)pLat[5]-48)/10 + ((double)pLat[6]-48)/100)/60;
				if (pNorS[0]=='S')	{
					location->latitude*=-1;
				}



				location->longitude=(pLong[0]-48)*100+(pLong[1]-48)*10 + (pLong[2]-48) + (((double)pLong[3]-48)*10 + ((double)pLong[4]-48) + ((double)pLong[6]-48)/10 + ((double)pLong[7]-48)/100)/60;
				if (pEorW[0]=='W')	{
					location->longitude*=-1;
				}

//				printf("\nTS: %i", location->timestampS);
				//printf("Lat:string%s calc:%i %i 0:%i 1:%i 2:%i 0:%i %i\n", pLat, location->latitude, (pLat[1]-48)*10+(pLat[2]-48), pLat[0], pLat[1], pLat[2], *pLat, *(pLat+1));

//				printf("\nLat: %f Long: %f", location->latitude, location->longitude);

//				printf("\nTime:%s  Stat:%s Lat:%s%s Long:%s%s Track:%s Date:%s", pUTCtime, pStatus, pLat, pNorS, pLong, pEorW, pTrack, pDate);
				return 1;
			}


		}
	}
	return 0;
}

int ReadLocationFromJson(LOCATIONHISTORY *lh, LOCATION *location)
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
				//printf("ts %s\n",x);
				if (x)	x=strchr(x,'\"')+1;
				if (x)  y=strchr(x,'\"')-3;
				y[0]=0;
				location->timestampS=strtol(x, NULL, 10);
				step++;
				//printf("%i\n",location->timestampS);
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
	fprintf (stdout, "Finished reading JSON file.\n");
	return 0;
};


int bitmapInit(BM* bm, OPTIONS* options, LOCATIONHISTORY *lh)	//set up the bitmap, and copy over all the critical options
{
	char* bitmap;

	bm->width=options->width;
	bm->height=options->height;
	bm->sizebitmap=options->width*options->height*4;

	CopyNSWE(&bm->nswe, &options->nswe);


	bitmap=(char*)calloc(bm->sizebitmap, sizeof(char));

//	printf("\nBitmap initiated %i. Width: %i, height: %i", bitmap, options->width, options->height);

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
	memcpy(&c,bm->bitmap+(x+y* bm->width) *4, 4);
	//c.R = bm->bitmap[(x+y* bm->width) *4];
	//c.G = bm->bitmap[(x+y* bm->width) *4+1];
	//c.B = bm->bitmap[(x+y* bm->width) *4+2];
	//c.A = bm->bitmap[(x+y* bm->width) *4+3];


	return c;
}

int bitmapPixelSet(BM* bm, int x, int y, COLOUR *c)
{
	COLOUR currentC;

	//should remove sanity testing from here as it's used often, needs to be checked by the calling function if required
	if (x>bm->width-1)	return 0;
	if (y>bm->height-1)	return 0;
	if (x<0)	return 0;
	if (y<0)	return 0;

	memcpy(&currentC, bm->bitmap+(x+y* bm->width) *4, sizeof(COLOUR));


	if (mixColours(&currentC, c))	{
			memcpy(bm->bitmap + (x + y * bm->width) * 4, &currentC,sizeof(COLOUR));	//copy the chunk over
	}

	return 1;
}

int bitmapPixelSetOld(BM* bm, int x, int y, COLOUR *c)
{
	COLOUR currentC;

	//should remove sanity testing from here as it's used often, needs to be checked by the calling function if required
	if (x>bm->width-1)	return 0;
	if (y>bm->height-1)	return 0;
	if (x<0)	return 0;
	if (y<0)	return 0;

	currentC.R = bm->bitmap[(x+y* bm->width) *4];
	currentC.G = bm->bitmap[(x+y* bm->width) *4+1];
	currentC.B = bm->bitmap[(x+y* bm->width) *4+2];
	currentC.A = bm->bitmap[(x+y* bm->width) *4+3];

//	memcpy(&currentC, bm->bitmap+(x+y* bm->width) *4, 4);


	if (mixColours(&currentC, c))	{
			memset(bm->bitmap + (x + y * bm->width) * 4+0,currentC.R,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+1,currentC.G,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+2,currentC.B,1);	//the *4 is the channels
			memset(bm->bitmap + (x + y * bm->width) * 4+3,currentC.A,1);	//the *4 is the channels

//			memcpy(bm->bitmap + (x + y * bm->width) * 4, &currentC,4);	//copy the chunk over
	}

	return 1;
}



int mixColours(COLOUR *cCanvas, COLOUR *cBrush)	//this alters the canvas
{
	int r,g,b,a;

	if ((cBrush->A)==0)	{//if we're painting with a transparent brush, just return
		return 1;
	}

	if ((cBrush->A)==255)	{	//if the brush is completely opaque
		cCanvas->R = cBrush->R;
		cCanvas->G = cBrush->G;
		cCanvas->B = cBrush->B;
		cCanvas->A = 255;
		return 1;
	}

	//first work out the output alpha
	a=cBrush->A + (cCanvas->A * (255 - cBrush->A))/255;	//proper way to do it, it's not simply additive

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
	//printf("r%i g%i b%i a%i\n",r,g,b,a);



	//set the canvas
	cCanvas->R=r;
	cCanvas->G=g;
	cCanvas->B=b;
	cCanvas->A=a;
	return 1;

}


int bitmapFilledCircle(BM* bm, int x, int y, int diameter, COLOUR *c)
{
	int row,col;
	int startcol;

	int oddwidth;

	double radsquared;
	double radplusonesquared;
	double distsq, nextcoldistsq;
	double dist;
	double diff;


	int cx,cy;
	int finished;

	int a;	//alpha

	if (diameter <1)
		return 0;


	cx=x+diameter/2;
	cy=y+diameter/2;

	radsquared = (double)diameter/2;
	radplusonesquared = radsquared+1;

	radsquared *= radsquared;
	radplusonesquared *= radplusonesquared;


	row=0;
	col=0;

	oddwidth=diameter & 1;
	if (oddwidth)	{	//odd diameter
		plot(bm,cx+col,cy+row,255,c);
		col++;
	}
	startcol=0;
	finished=0;

	while (!finished)	{
		if (oddwidth)	{
			distsq = (col)*(col) + (row)*(row);
			nextcoldistsq = (col+1)*(col+1)+(row)*(row);
		}
		else	{
			distsq = (col+0.5)*(col+0.5) + (row+0.5)*(row+0.5);	//for even circles, the starting row col is actually half a diagonal pixel away
			nextcoldistsq = (col+1.5)*(col+1.5)+(row+0.5)*(row+0.5);
		}
//		printf("%.2f,", (double)dist);
		if ((distsq < radsquared))	{
			if (nextcoldistsq>radsquared)	{	//test the next pixel, only calculate exact distance if the next is outside the circle
				dist = sqrt(distsq);
				if (oddwidth)	{
					diff = diameter/2 - dist;
				}
				else	//if even width
					diff = diameter/2 - dist-0.5;
				if (diff <0)	{	//if the full circle wouldn't be visible
					a=255*(1+diff);
				}
				else {	//if the diff is positive we need another (usually faint) point
					a=diff*255;
					plotOctet(bm, cx, cy, col+1, row, oddwidth, a, c);
					a=255;
				}

			}
			else	{
				a=255;
			}


			//plot the main point
			plotOctet(bm, cx, cy, col, row, oddwidth, a, c);

			//printf("\n%i,%i %i %i", col,row,cx,cy);
			col++;
		}
		else {
			if (startcol==col)
				finished=1;
			startcol++;
			col=startcol;
			row++;
//			printf("\n");
		}
	}



	return 1;
}


void plotOctet(BM* bm, int cx, int cy, int col, int row, int isoddwidth, int alpha, COLOUR *c)
{
	if (isoddwidth)	{
		plot(bm,cx+col,cy+row,alpha,c);
		if (row!=col)	{
			plot(bm,cx+row,cy+col,alpha,c);
			plot(bm,cx+row,cy-col,alpha,c);
			plot(bm,cx-col,cy+row,alpha,c);
		}

		if (row>0)	{
			plot(bm,cx+col,cy-row,alpha,c);
			plot(bm,cx-col,cy-row,alpha,c);
			plot(bm,cx-row,cy+col,alpha,c);
			plot(bm,cx-row,cy-col,alpha,c);
		}
	} else	{	//if it's even, there's no middle pixel, so we mirror and reflect differently
		plot(bm,cx+col,cy+row,alpha,c);
		if (row!=col)	{
			plot(bm,cx-col-1,cy+row,alpha,c);
			plot(bm,cx+row,cy+col,alpha,c);
			plot(bm,cx+row,cy-col-1,alpha,c);
		}
		if (row>-1)	{
			plot(bm,cx+col,cy-row-1,alpha,c);
			plot(bm,cx-row-1,cy+col,alpha,c);
			plot(bm,cx-row-1,cy-col-1,alpha,c);
			plot(bm,cx-col-1,cy-row-1,alpha,c);
		}
	}


	return;
}

int bitmapSquare(BM* bm, int x0, int y0, int x1, int y1, COLOUR *cBorder, COLOUR *cFill)
{
	int x,y;

	//Draw the border
	if (cBorder)	{
		//top border
		if (y0>=0)	{	//if it starts within the canvas
			y=y0;
			for (x=x0;x<=x1;x++)	{	//draw the top line
				bitmapPixelSet(bm, x,y, cBorder);
			}
		}
		else	{	//otherwise set it to just outside
			y0=-1;
		}

		//bottom border
		if	(y1<bm->height)	{
			y=y1;
			for (x=x0;x<=x1;x++)	{
				bitmapPixelSet(bm, x,y, cBorder);
			}
		} else y1=bm->height;	//again, just outside
		//left border
		x=x0;
		for (y=y0+1;y<y1;y++)	{	//note we don't have to redraw the very end pixel of the sides
			bitmapPixelSet(bm, x,y, cBorder);
		}
		//right border
		x=x1;
		for (y=y0+1;y<y1;y++)	{
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


	if (thickness >1)	{
		hypotenusethickness=thickness*sqrt(gradient*gradient+1);
		r=0-hypotenusethickness/2;
		f=(hypotenusethickness+1)/2;
	}
	else {
		r=0;
		f=1;
	}


	if (abs(gradient)>1)	{
		printf("\n Gradient>1 (%.1f, %.1f) (%.1f, %.1f) %.2f\t %i\t %.4f \n", x0,y0, x1,y1, hypotenusethickness, thickness,gradient);
	}

	//Convert to int and round;

	x0=(int)(x0+0.5);
	y0=(int)(y0+0.5);
	x1=(int)(x1+0.5);
	y1=(int)(y1+0.5);

	//why round?
	x0=(int)(x0);
	y0=(int)(y0);
	x1=(int)(x1);
	y1=(int)(y1);


	unsigned char rstrength, fstrength;	//the intensity of the fills

	// handle first endpoint

	if (thickness>1)	{
		if (steep)	{
			bitmapFilledCircle(bm,y0+r,x0+r,thickness,c);
		}
		else
			bitmapFilledCircle(bm,x0+r,y0+r,thickness,c);
	}



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
	if ((lon1 < bm->nswe.west) && (lon2 < bm->nswe.west))	return 0;
	if ((lon1 > bm->nswe.east) && (lon2 > bm->nswe.east))	return 0;
	if ((lat1 < bm->nswe.south) && (lat2 < bm->nswe.south))	return 0;
	if ((lat1 > bm->nswe.north) && (lat2 > bm->nswe.north))	return 0;


	LatLongToXY(bm, lat1, lon1, &x1,&y1);
	LatLongToXY(bm, lat2, lon2, &x2,&y2);
	dx=x1-x2;
	dy=y1-y2;
	if ((abs(dx)>180*bm->zoom))	{	//they've moved over half the map
		swappedflag=0;	//flag
		if (dx >0)	{	//if it's the eastward direction then we'll swap vars
			//printf("Pretransform %i %i to %i %i, diff:%i %i c: %i %i\n", x2, y2, xi,yi,dx,dy, 0,yintersect);
			tempdouble=x1;x1=x2;x2=tempdouble;	//swap x
			tempdouble=y1;y1=y2;y2=tempdouble;	//swap y

			dx=x1-x2;	dy=y1-y2;
			swappedflag=1;	//flag to ensure we swap coords back
		}

		dx=x1-(x2-360*bm->zoom);	//x2-360*zoom is our new origin
		m=dy;	m/=dx;				//gradient, done this ugly way as doing division on ints

		yintersect = y2 + (360*bm->zoom-x2)*m;
//				printf("from %i %i to %i %i, diff:%i %i yint: %i m:%f\n", x2, y2, xi,yi,dx,dy, yintersect,m);

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
		//printf("bcl %f, %f to %f %f\n",x2,y2,x1,y1);
		return bitmapLineDrawWu(bm, x2,y2,x1,y1,thickness, c);		//the normal case
	}


	return 1;
}

int DrawRegion(BM *bm, WORLDREGION *r)
{

	double x0, y0;
	double x1, y1;
	COLOUR cFill;

	LatLongToXY(bm, r->nswe.north, r->nswe.west, &x0, &y0);
	LatLongToXY(bm, r->nswe.south, r->nswe.east, &x1, &y1);


	cFill.R = r->baseColour.R;
	cFill.G = r->baseColour.G;
	cFill.B = r->baseColour.B;
	cFill.A = r->baseColour.A*0.8;

	bitmapSquare(bm, x0,y0, x1,y1, &r->baseColour, &cFill);

	return 0;
}

int DrawListOfRegions(BM *bm, WORLDREGION *first)
{
	WORLDREGION * r;
	r=first;
	while (r)	{
		DrawRegion(bm, r);
		r=r->next;
	}

	return 1;

}


int bitmapWrite(BM *bm, char *filename)	//this writes as a raw file
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

	*y=(bm->nswe.north - phi)/(bm->nswe.north - bm->nswe.south)*height;
	*x=(lambda - bm->nswe.west)/(bm->nswe.east - bm->nswe.west)*width;

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

//	printf("%i\t%i\t%i\t%f\n", ts, diff, ts-min, hue);
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
//		printf("%f %f; %f %f\n",x1,y1,x2,y2);
		bitmapLineDrawWu(bm, x1,y1,x2,y2,1, &c);
	}

	for (lon=-180+spacing; lon<180; lon+=spacing)	{
		LatLongToXY(bm, -90, lon, &x1, &y1);
		LatLongToXY(bm, 90, lon, &x2, &y2);
//		printf("%f %f; %f %f\n",x1,y1,x2,y2);
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
	fprintf(kml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(kml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
	fprintf(kml, "<GroundOverlay>\n");
	fprintf(kml, "<name>WorldTracker - %s</name>\n",bm->options->title);
	fprintf(kml, "<description>");
	fprintf(kml, "From %s to %s.\n", sStartTime, sEndTime);
	fprintf(kml, "Plotted %i points.\n", bm->countPoints);
	fprintf(kml, "</description>\n");
	fprintf(kml, "<Icon><href>%s</href></Icon>\n",pngnameNoDirectory);
    fprintf(kml, "<LatLonBox>\n");
	fprintf(kml, "<north>%f</north>\n",bm->nswe.north);
	fprintf(kml, "<south>%f</south>\n",bm->nswe.south);
	fprintf(kml, "<east>%f</east>\n",bm->nswe.east);
	fprintf(kml, "<west>%f</west>\n",bm->nswe.west);
	fprintf(kml, "<rotation>0</rotation>\n");
	fprintf(kml, "</LatLonBox>\n");
	fprintf(kml, "</GroundOverlay>\n");
	fprintf(kml, "</kml>\n");

	fclose(kml);

	return 1;
}

int ExportGPXFile(LOCATIONHISTORY *lh, char * GPXFilename, unsigned long tsfrom, unsigned long tsto)
{
	FILE *gpx;
	LOCATION *loc;
	struct tm time;
	char stringTime[255];

	gpx = fopen(GPXFilename, "w");

	if (!gpx)	{
		return 0;
	}


	fprintf(gpx, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(gpx, "<gpx version=\"1.1\" creator=\"WorldTracker\">\n");

	fprintf(gpx, "<metadata>\n");
	fprintf(gpx, "<link>https://github.com/tburtenshaw/worldtracker</link>\n");
	fprintf(gpx, "<name>WorldTracker GPX Export</name>\n");

	strftime(stringTime,255,"%Y-%m-%d", gmtime_s(&tsfrom, &time));
	fprintf(gpx, "<desc>Created by WorldTracker, from %s to ", stringTime);
	strftime(stringTime,255,"%Y-%m-%d", gmtime_s(&tsto, &time));
	fprintf(gpx, "%s</desc>\n", stringTime);

	fprintf(gpx, "<author><name>WorldTracker</name></author>\n");
	fprintf(gpx, "</metadata>\n");

	//Write the location data to a track
	fprintf(gpx, "<trk>\n");

	fprintf(gpx, "<trkseg>\n");
	loc=lh->first;
	while (loc)	{
		if ((loc->timestampS >= tsfrom) && (loc->timestampS <= tsto))	{
		fprintf(gpx, "<trkpt lat=\"%f\" lon=\"%f\">\n",loc->latitude, loc->longitude);
		strftime(stringTime,255,"%Y-%m-%dT%H:%M:%SZ",gmtime_s(&loc->timestampS, &time));
		fprintf(gpx, "<time>%s</time>\n",stringTime);
     	fprintf(gpx, "</trkpt>\n");
		}

		loc=loc->next;
	}
	fprintf(gpx, "</trkseg>\n");

	fprintf(gpx, "</trk>\n");

	fprintf(gpx, "</gpx>\n");

	fclose(gpx);
	return 1;
}

int GetPresetScoreArray(char *searchtext, PRESET *presetlist, int countlist, int *scorearray);

int GetPresetScoreArray(char *searchtext, PRESET *presetlist, int countlist, int *score)
{
	char searchphrase[256];
	char *searchterm;
	char *result;
	char *resultupper;
	char *resultlower;


	for (int i=0; i<countlist; i++)	{
		score[i]=0;

		strcpy(searchphrase, searchtext);

		if (strcmpi(searchphrase, presetlist[i].name) == 0)	{	//if it's the whole phrase, then more points
			//printf("\nphrase %s", searchphrase);
			score[i]+=501;
		}


		searchterm = strtok(searchphrase, " ");
		//printf("\nterm %s", searchterm);

		while (searchterm)	{
		//	printf("\nSearching %i %s from %s", i, searchterm, searchphrase);

			if (strcmpi(searchterm, presetlist[i].abbrev) == 0)	{
				score[i]+=500;
			}
			if (strcmpi(searchterm, presetlist[i].name) == 0)	{
				score[i]+=500;
			}

			result = stristr(presetlist[i].name, searchterm);
			if (result)	{
				score[i]+=50;
			}
	//		printf("\n %i r:%i diff:%i", result, presetlist[i].name,  result- presetlist[i].name);
			if (result ==presetlist[i].name)	{	//if it's a match from the beginning character it's worth more
				score[i]+=100;
			}
			if (result>presetlist[i].name)	{	//before a space
				if (*(result-1) == ' ')	{
					score[i]+=60;
				}
			}

			result = stristr(presetlist[i].abbrev, searchterm);
			if (result)	{
				score[i]+=30;
			}
			if (result ==presetlist[i].abbrev)	{	//if it's a match from the beginning character it's worth more
				score[i]+=90;
			}

			//look for three letter runs
			char threelength[4];
			if (strlen(searchterm)>3)	{
				for (unsigned int n=0; n<strlen(searchterm)-3;n++)	{
					threelength[0] = searchterm[n];
					threelength[1] = searchterm[n+1];
					threelength[2] = searchterm[n+2];
					threelength[3] = 0;

					if (stristr(presetlist[i].name, threelength))	{
						score[i]+=3;
					}

				}
			}

			//now just count the shared character, this can include small typos that would otherwise be lost
			for (unsigned int n=0; n<strlen(searchterm);n++)	{
				result = strchr(presetlist[i].name, searchterm[n]);
				while (result)	{
					score[i]+=1;
					result = strchr(result+1, searchterm[n]);
				}
			}
			score[i]-=(strlen(presetlist[i].name)/2);	//so long names aren't unfairly advantaged.

			//this checks for abbreviations, if the search term is shorter than the name, it checks for letters in order (e.g. WGTN = WellinGToN)
			int n=0;
			result = presetlist[i].name;
			while (result)	{
					resultupper  = strchr(result, toupper(searchterm[n]));
					resultlower  = strchr(result, tolower(searchterm[n]));

					if (resultupper && resultlower)	{
						if (resultupper<resultlower)	{
							result = resultupper;
							}
						else	{
							result = resultlower;
						}
					}
					else if (resultupper)	{
						result = resultupper;
					}
					else	{
						result = resultlower;
					}


					if (result)	{
						score[i]+=3;
						if (n==0)	{score[i]+=3;}	//bonus if first char
						n++;
					}
			}

//			for (unsigned int n=0; n<strlen(searchterm);n++)	{

//				result = strchr(presetlist[i].name+n, toupper(searchterm[n]));
//				if (!result)
//				if (strchr(presetlist[i].name+n, toupper(searchterm[n])) || strchr(presetlist[i].name+n, tolower(searchterm[n])) )	{
//					score[i]+=3;
//				}
//			}

//
//			if (score[i]>0)	{
//				printf("\n%s: score:%i", presetlist[i].name,score[i]);
//			}

			searchterm = strtok(NULL, " ");
		}//close while
	}//close for
	return 0;

}

int GetBestPresets(char *searchtext, PRESET *presetlist, int countlist, PRESET *presetbest, int countbest)
{
	int i;
	int score[countlist];

	//Get the scores from another fuction
	GetPresetScoreArray(searchtext, presetlist, countlist, score);

	//Now we find the best scores
	int scorebest[countbest];

	//set everything to zero
	memset(scorebest, 0, countbest*sizeof(int));
	memset(presetbest, 0, countbest*sizeof(PRESET));


	//sort the top scores
	for (i=0; i<countlist; i++)	{
		if (score[i]>scorebest[countbest-1])	{
			//we add this preset to the list
			for (int n=0; n<countbest; n++)	{
				if (score[i]>scorebest[n])	{	//test each of the best
					for (int x=countbest-1; x>n; x--)	{	//move down everything
						scorebest[x] = scorebest[x-1];
						memcpy(&presetbest[x], &presetbest[x-1], sizeof(PRESET));
					}
					//printf("\nIn position: %i, %s (%i) becomes %s (%i)", n, presetbest[n].name,scorebest[n], presetlist[i].name,score[i]);
					scorebest[n]=score[i];
					memcpy(&presetbest[n], &presetlist[i], sizeof(PRESET));
					//printf(", then %i: %s is %s", n, presetbest[n].name, presetlist[i].name);
					n=countbest;	//end the search
				}
			}
		}

	}


	//return the number in the array
	for (i=0;i<countbest;i++)	{
//		printf("\nscorebest[%i] = %i %s", i, scorebest[i], presetbest[i].name);
		if (scorebest[i] == 0)	{
//			printf("\nreturned: %i", i);
			return i;
		}
	}
//	printf("\nreturned: %i", countbest);
	return countbest;
}

void LoadPresets(PRESET *preset, int * pCount, int maxCount)
{
	int i;

	preset[0].abbrev="nz";preset[0].nswe.north=-34;preset[0].nswe.south=-47.5;preset[0].nswe.west=166;preset[0].nswe.east=178.5;preset[0].name="New Zealand";
	preset[1].abbrev="northisland";preset[1].nswe.north=-34.37;preset[1].nswe.south=-41.62;preset[1].nswe.west=172.6;preset[1].nswe.east=178.6;preset[1].name="North Island";
	preset[2].abbrev="auckland";preset[2].nswe.north=-36.7;preset[2].nswe.south=-37.1;preset[2].nswe.west=174.5;preset[2].nswe.east=175;preset[2].name="Auckland";
	preset[3].abbrev="aucklandcentral";preset[3].nswe.north=-36.835;preset[3].nswe.south=-36.935;preset[3].nswe.west=174.69;preset[3].nswe.east=174.89;preset[3].name="Central Auckland";
	preset[4].abbrev="tauranga";preset[4].nswe.north=-37.6;preset[4].nswe.south=-37.76;preset[4].nswe.west=176.07;preset[4].nswe.east=176.36;preset[4].name="Tauranga";
	preset[5].abbrev="wellington";preset[5].nswe.north=-41.06;preset[5].nswe.south=-41.4;preset[5].nswe.west=174.6;preset[5].nswe.east=175.15;preset[5].name="Wellington";
	preset[6].abbrev="christchurch";preset[6].nswe.north=-43.43;preset[6].nswe.south=-43.62;preset[6].nswe.west=172.5;preset[6].nswe.east=172.81;preset[6].name="Christchurch";
	preset[7].abbrev="queenstown";preset[7].nswe.north=-44.5;preset[7].nswe.south=-45.6;preset[7].nswe.west=168;preset[7].nswe.east=169.5;preset[7].name="Queenstown";
	preset[8].abbrev="dunedin";preset[8].nswe.north=-45.7;preset[8].nswe.south=-45.95;preset[8].nswe.west=170.175;preset[8].nswe.east=170.755;preset[8].name="Dunedin";
	preset[9].abbrev="au";preset[9].nswe.north=-10.5;preset[9].nswe.south=-44;preset[9].nswe.west=112;preset[9].nswe.east=154;preset[9].name="Australia";
	preset[10].abbrev="queensland";preset[10].nswe.north=-9.5;preset[10].nswe.south=-29;preset[10].nswe.west=138;preset[10].nswe.east=154;preset[10].name="Queensland";
	preset[11].abbrev="sydney";preset[11].nswe.north=-33.57;preset[11].nswe.south=-34.14;preset[11].nswe.west=150.66;preset[11].nswe.east=151.35;preset[11].name="Sydney";
	preset[12].abbrev="asia";preset[12].nswe.north=58;preset[12].nswe.south=-11;preset[12].nswe.west=67;preset[12].nswe.east=155;preset[12].name="Asia";
	preset[13].abbrev="hk";preset[13].nswe.north=23.2;preset[13].nswe.south=21.8;preset[13].nswe.west=112.8;preset[13].nswe.east=114.7;preset[13].name="Hong Kong";
	preset[14].abbrev="sg";preset[14].nswe.north=1.51;preset[14].nswe.south=1.15;preset[14].nswe.west=103.6;preset[14].nswe.east=104.1;preset[14].name="Singapore";
	preset[15].abbrev="in";preset[15].nswe.north=37;preset[15].nswe.south=6;preset[15].nswe.west=67.65;preset[15].nswe.east=92.56;preset[15].name="India";
	preset[16].abbrev="jp";preset[16].nswe.north=45.75;preset[16].nswe.south=30.06;preset[16].nswe.west=128.35;preset[16].nswe.east=149.09;preset[16].name="Japan";
	preset[17].abbrev="europe";preset[17].nswe.north=55;preset[17].nswe.south=36;preset[17].nswe.west=-10;preset[17].nswe.east=32;preset[17].name="Europe";
	preset[18].abbrev="es";preset[18].nswe.north=44;preset[18].nswe.south=35;preset[18].nswe.west=-10.0;preset[18].nswe.east=5;preset[18].name="Spain";
	preset[19].abbrev="it";preset[19].nswe.north=47;preset[19].nswe.south=36.5;preset[19].nswe.west=6.6;preset[19].nswe.east=19;preset[19].name="Italy";
	preset[20].abbrev="venice";preset[20].nswe.north=45.6;preset[20].nswe.south=45.3;preset[20].nswe.west=12.1;preset[20].nswe.east=12.6;preset[20].name="Venice";
	preset[21].abbrev="fr";preset[21].nswe.north=51.2;preset[21].nswe.south=42.2;preset[21].nswe.west=-5.5;preset[21].nswe.east=8.5;preset[21].name="France";
	preset[22].abbrev="paris";preset[22].nswe.north=49.1;preset[22].nswe.south=48.5;preset[22].nswe.west=1.8;preset[22].nswe.east=2.8;preset[22].name="Paris";
	preset[23].abbrev="uk";preset[23].nswe.north=60;preset[23].nswe.south=50;preset[23].nswe.west=-10.5;preset[23].nswe.east=2;preset[23].name="United Kingdom";
	preset[24].abbrev="scandinaviabaltic";preset[24].nswe.north=71.5;preset[24].nswe.south=53.5;preset[24].nswe.west=4.3;preset[24].nswe.east=41.7;preset[24].name="Scandinavia and Baltic";
	preset[25].abbrev="is";preset[25].nswe.north=66.6;preset[25].nswe.south=63.2;preset[25].nswe.west=-24.6;preset[25].nswe.east=-13.5;preset[25].name="Iceland";
	preset[26].abbrev="cz";preset[26].nswe.north=51.1;preset[26].nswe.south=48.5;preset[26].nswe.west=12;preset[26].nswe.east=18.9;preset[26].name="Czech Republic";
	preset[27].abbrev="prague";preset[27].nswe.north=50.178;preset[27].nswe.south=49.941;preset[27].nswe.west=14.246;preset[27].nswe.east=14.709;preset[27].name="Prague";
	preset[28].abbrev="vienna";preset[28].nswe.north=48.3;preset[28].nswe.south=48.12;preset[28].nswe.west=16.25;preset[28].nswe.east=16.55;preset[28].name="Vienna";
	preset[29].abbrev="turkeygreece";preset[29].nswe.north=42.294;preset[29].nswe.south=34.455;preset[29].nswe.west=19.33;preset[29].nswe.east=45.09;preset[29].name="Turkey and Greece";
	preset[30].abbrev="istanbul";preset[30].nswe.north=41.3;preset[30].nswe.south=40.7;preset[30].nswe.west=28.4;preset[30].nswe.east=29.7;preset[30].name="Istanbul";
	preset[31].abbrev="middleeast";preset[31].nswe.north=42;preset[31].nswe.south=12;preset[31].nswe.west=25;preset[31].nswe.east=69;preset[31].name="Middle East";
	preset[32].abbrev="uae";preset[32].nswe.north=26.5;preset[32].nswe.south=22.6;preset[32].nswe.west=51.5;preset[32].nswe.east=56.6;preset[32].name="United Arab Emirates";
	preset[33].abbrev="dubai";preset[33].nswe.north=25.7;preset[33].nswe.south=24.2;preset[33].nswe.west=54.2;preset[33].nswe.east=55.7;preset[33].name="Dubai";
	preset[34].abbrev="israeljordan";preset[34].nswe.north=33.4;preset[34].nswe.south=29.1;preset[34].nswe.west=34;preset[34].nswe.east=39.5;preset[34].name="Israel and Jordan";
	preset[35].abbrev="usane";preset[35].nswe.north=47.5;preset[35].nswe.south=36.5;preset[35].nswe.west=-82.7;preset[35].nswe.east=-67;preset[35].name="North Eastern USA";
	preset[36].abbrev="usa";preset[36].nswe.north=49;preset[36].nswe.south=24;preset[36].nswe.west=-125;preset[36].nswe.east=-67;preset[36].name="United States of America";
	preset[37].abbrev="boston";preset[37].nswe.north=42.9;preset[37].nswe.south=42;preset[37].nswe.west=-71.9;preset[37].nswe.east=-70.5;preset[37].name="Boston";
	preset[38].abbrev="arkansas";preset[38].nswe.north=36.5;preset[38].nswe.south=33;preset[38].nswe.west=-94.6;preset[38].nswe.east=-89;preset[38].name="Arkansas";
	preset[39].abbrev="lasvegas";preset[39].nswe.north=36.35;preset[39].nswe.south=35.9;preset[39].nswe.west=-115.35;preset[39].nswe.east=-114.7;preset[39].name="Las Vegas";
	preset[40].abbrev="oceania";preset[40].nswe.north=35;preset[40].nswe.south=-50;preset[40].nswe.west=-220;preset[40].nswe.east=-110;preset[40].name="Oceania";
	preset[41].abbrev="fiji";preset[41].nswe.north=-15.5;preset[41].nswe.south=-21.14;preset[41].nswe.west=176.77;preset[41].nswe.east=182.08;preset[41].name="Fiji";
	preset[42].abbrev="montreal";preset[42].nswe.north=45.711;preset[42].nswe.south=45.373;preset[42].nswe.west=-73.99;preset[42].nswe.east=-73.374;preset[42].name="Montreal";
	preset[43].abbrev="southeastasia";preset[43].nswe.north=29;preset[43].nswe.south=-11;preset[43].nswe.west=91;preset[43].nswe.east=128;preset[43].name="South East Asia";
	preset[44].abbrev="th";preset[44].nswe.north=20.5;preset[44].nswe.south=5.5;preset[44].nswe.west=97;preset[44].nswe.east=106;preset[44].name="Thailand";
	preset[45].abbrev="florida";preset[45].nswe.north=31;preset[45].nswe.south=24.4;preset[45].nswe.west=-87.65;preset[45].nswe.east=-80;preset[45].name="Florida";
	preset[46].abbrev="world";preset[46].nswe.north=90;preset[46].nswe.south=-90;preset[46].nswe.west=-180;preset[46].nswe.east=180;preset[46].name="The World";
	preset[47].abbrev="africa";preset[47].nswe.north=37.7;preset[47].nswe.south=-37;preset[47].nswe.west=-20;preset[47].nswe.east=53;preset[47].name="Africa";
	preset[48].abbrev="southamerica";preset[48].nswe.north=15;preset[48].nswe.south=-59;preset[48].nswe.west=-84;preset[48].nswe.east=-32;preset[48].name="South America";

	i=49;
	preset[i].abbrev="saaf";preset[i].nswe.north=38;preset[i].nswe.south=-60;preset[i].nswe.west=-86;preset[i].nswe.east=55;preset[i].name="South America and Africa";i++;
	preset[i].abbrev="northamerica";preset[i].nswe.north=77;preset[i].nswe.south=14;preset[i].nswe.west=-167;preset[i].nswe.east=-52;preset[i].name="North America";i++;
	preset[i].abbrev="bh";preset[i].nswe.north=26.5;preset[i].nswe.south=25.7;preset[i].nswe.west=50.2;preset[i].nswe.east=51;preset[i].name="Bahrain";i++;
	preset[i].abbrev="de";preset[i].nswe.north=55;preset[i].nswe.south=47;preset[i].nswe.west=5.5;preset[i].nswe.east=15;preset[i].name="Germany";i++;
	preset[i].abbrev="ru";preset[i].nswe.north=78;preset[i].nswe.south=43;preset[i].nswe.west=27;preset[i].nswe.east=190;preset[i].name="Russia";i++;
	preset[i].abbrev="europeeast";preset[i].nswe.north=78;preset[i].nswe.south=38;preset[i].nswe.west=12;preset[i].nswe.east=90;preset[i].name="Eastern Europe";i++;
	preset[i].abbrev="london";preset[i].nswe.north=51.7;preset[i].nswe.south=51.25;preset[i].nswe.west=-0.6;preset[i].nswe.east= 0.35;preset[i].name="London";i++;
	preset[i].abbrev="mg";preset[i].nswe.north=-11.5;preset[i].nswe.south=-26;preset[i].nswe.west=43;preset[i].nswe.east=51;preset[i].name="Madagascar";i++;
	preset[i].abbrev="auwa";preset[i].nswe.north=-13.5;preset[i].nswe.south=-35.5;preset[i].nswe.west=112.5;preset[i].nswe.east=129;preset[i].name="Western Australia";i++;
	preset[i].abbrev="za";preset[i].nswe.north=-22;preset[i].nswe.south=-35;preset[i].nswe.west=14;preset[i].nswe.east=33;preset[i].name="South Africa";i++;
	preset[i].abbrev="si";preset[i].nswe.north=46.9;preset[i].nswe.south=45.4;preset[i].nswe.west=13.3;preset[i].nswe.east=16.65;preset[i].name="Slovenia";i++;
	preset[i].abbrev="balkans";preset[i].nswe.north=48;preset[i].nswe.south=36;preset[i].nswe.west=13;preset[i].nswe.east=31;preset[i].name="The Balkans";i++;
	preset[i].abbrev="ua";preset[i].nswe.north=53.5;preset[i].nswe.south=45;preset[i].nswe.west=22;preset[i].nswe.east=40.5;preset[i].name="Ukraine";i++;
	preset[i].abbrev="ca";preset[i].nswe.north=75;preset[i].nswe.south=42;preset[i].nswe.west=-141;preset[i].nswe.east=-51;preset[i].name="Canada";i++;
	preset[i].abbrev="greatlakes";preset[i].nswe.north=51;preset[i].nswe.south=41;preset[i].nswe.west=-93;preset[i].nswe.east=-75;preset[i].name="The Great Lakes";i++;
	preset[i].abbrev="centralamerica";preset[i].nswe.north=18.6;preset[i].nswe.south=7;preset[i].nswe.west=-92.5;preset[i].nswe.east=-77;preset[i].name="Central America";i++;
	preset[i].abbrev="pt";preset[i].nswe.north=42.2;preset[i].nswe.south=36.9;preset[i].nswe.west=-9.6;preset[i].nswe.east=-6;preset[i].name="Portugal";i++;
	preset[i].abbrev="amsterdam";preset[i].nswe.north=52.46;preset[i].nswe.south=52.25;preset[i].nswe.west=4.58;preset[i].nswe.east=5.085;preset[i].name="Amsterdam";i++;
	preset[i].abbrev="northafrica";preset[i].nswe.north=37.4;preset[i].nswe.south=11;preset[i].nswe.west=-18;preset[i].nswe.east=39;preset[i].name="North Africa";i++;
	preset[i].abbrev="in_goa";preset[i].nswe.north=15.8;preset[i].nswe.south=14.85;preset[i].nswe.west=73.65;preset[i].nswe.east=74.35;preset[i].name="Goa";i++;
	preset[i].abbrev="in_agra";preset[i].nswe.north=27.298;preset[i].nswe.south=27.097;preset[i].nswe.west=77.87;preset[i].nswe.east=78.12;preset[i].name="Agra";i++;
	preset[i].abbrev="kr";preset[i].nswe.north=38.8;preset[i].nswe.south=33;preset[i].nswe.west=124.5;preset[i].nswe.east=129.8;preset[i].name="South Korea";i++;
	preset[i].abbrev="ie";preset[i].nswe.north=55.5;preset[i].nswe.south=51.3;preset[i].nswe.west=-10.8;preset[i].nswe.east=-5.3;preset[i].name="Ireland";i++;
	preset[i].abbrev="cy";preset[i].nswe.north=35.8;preset[i].nswe.south=34.5;preset[i].nswe.west=32;preset[i].nswe.east=34.7;preset[i].name="Cyprus";i++;
	preset[i].abbrev="es_ibiza";preset[i].nswe.north=39.15;preset[i].nswe.south=38.6;preset[i].nswe.west=1.1;preset[i].nswe.east=1.7;preset[i].name="Ibiza";i++;
	preset[i].abbrev="us_nm";preset[i].nswe.north=37;preset[i].nswe.south=31;preset[i].nswe.west=-115.05;preset[i].nswe.east=-109.044;preset[i].name="New Mexico";i++;
	preset[i].abbrev="us_az";preset[i].nswe.north=37;preset[i].nswe.south=31.33;preset[i].nswe.west=-109.05;preset[i].nswe.east=-103;preset[i].name="Arizona";i++;
	preset[i].abbrev="id_bali";preset[i].nswe.north=-8;preset[i].nswe.south=-9;preset[i].nswe.west=114.25;preset[i].nswe.east=115.75;preset[i].name="Bali";i++;
	preset[i].abbrev="us_hi";preset[i].nswe.north=22.3;preset[i].nswe.south=18.8;preset[i].nswe.west=-160.6;preset[i].nswe.east=-154.7;preset[i].name="Hawaii";i++;
	preset[i].abbrev="greenland";preset[i].nswe.north=84;preset[i].nswe.south=59.5;preset[i].nswe.west=-71.5;preset[i].nswe.east=-10.3;preset[i].name="Greenland and Iceland";i++;
	preset[i].abbrev="mn";preset[i].nswe.north=52.2;preset[i].nswe.south=41.5;preset[i].nswe.west=87.2;preset[i].nswe.east=120.1;preset[i].name="Mongolia";i++;
	preset[i].abbrev="lk";preset[i].nswe.north=10;preset[i].nswe.south=5.8;preset[i].nswe.west=78;preset[i].nswe.east=82;preset[i].name="Sri Lanka";i++;
	preset[i].abbrev="ug";preset[i].nswe.north=4.2;preset[i].nswe.south=-1.5;preset[i].nswe.west=29.5;preset[i].nswe.east=35.1;preset[i].name="Uganda";i++;
	preset[i].abbrev="ke";preset[i].nswe.north=5;preset[i].nswe.south=-4.7;preset[i].nswe.west=33.5;preset[i].nswe.east=42;preset[i].name="Kenya";i++;
	preset[i].abbrev="us_al";preset[i].nswe.north=35.1;preset[i].nswe.south=30.1;preset[i].nswe.west=-88.5;preset[i].nswe.east=-84.8;preset[i].name="Alabama";i++;
	preset[i].abbrev="us_littlerock";preset[i].nswe.north=34.93;preset[i].nswe.south=34.5;preset[i].nswe.west=-92.65;preset[i].nswe.east=-92.05;preset[i].name="Little Rock";i++;
	preset[i].abbrev="in_gujarat";preset[i].nswe.north=24.7;preset[i].nswe.south=20;preset[i].nswe.west=68;preset[i].nswe.east=74.5;preset[i].name="Gujarat";i++;
	preset[i].abbrev="bd";preset[i].nswe.north=26.7;preset[i].nswe.south=20.5;preset[i].nswe.west=88;preset[i].nswe.east=92.7;preset[i].name="Bangladesh";i++;
	preset[i].abbrev="hornofafrica";preset[i].nswe.north=13;preset[i].nswe.south=-2;preset[i].nswe.west=38;preset[i].nswe.east=55;preset[i].name="Horn of Africa";i++;
	preset[i].abbrev="ma";preset[i].nswe.north=36;preset[i].nswe.south=27;preset[i].nswe.west=-13;preset[i].nswe.east=-1;preset[i].name="Morocco";i++;
	preset[i].abbrev="es_islascanarias";preset[i].nswe.north=29.5;preset[i].nswe.south=27.5;preset[i].nswe.west=-18.3;preset[i].nswe.east=-13.3;preset[i].name="Canary Islands";i++;
	preset[i].abbrev="westafrica";preset[i].nswe.north=27;preset[i].nswe.south=4;preset[i].nswe.west=-18;preset[i].nswe.east=16;preset[i].name="West Africa";i++;
	preset[i].abbrev="me";preset[i].nswe.north=43.6;preset[i].nswe.south=41.8;preset[i].nswe.west=18.4;preset[i].nswe.east=20.4;preset[i].name="Montenegro";i++;
	preset[i].abbrev="americas";preset[i].nswe.north=75;preset[i].nswe.south=-57;preset[i].nswe.west=-170;preset[i].nswe.east=-52;preset[i].name="The Americas";i++;
	preset[i].abbrev="sa";preset[i].nswe.north=-13.4;preset[i].nswe.south=-14.1;preset[i].nswe.west=-172.85;preset[i].nswe.east=-171.35;preset[i].name="Samoa";i++;
	preset[i].abbrev="uy";preset[i].nswe.north=-30;preset[i].nswe.south=-35;preset[i].nswe.west=-59;preset[i].nswe.east=-53;preset[i].name="Uruguay";i++;
	preset[i].abbrev="fk";preset[i].nswe.north=-51;preset[i].nswe.south=-52.5;preset[i].nswe.west=-61.5;preset[i].nswe.east=-57.6;preset[i].name="Falkland Islands";i++;
	preset[i].abbrev="us_la_greaterneworleans";preset[i].nswe.north=30.4;preset[i].nswe.south=28.9;preset[i].nswe.west=-91;preset[i].nswe.east=-88.8;preset[i].name="Greater New Orleans";i++;
	*pCount=i;
	return;
}

int NsweFromPreset(OPTIONS *options, char *lookuppreset, PRESET * presetarray, int numberofpresets)
{
	int i;

	for (i=0;i<numberofpresets;i++)	{
		if (!stricmp(lookuppreset, presetarray[i].name))	{
			options->nswe.north =presetarray[i].nswe.north;
			options->nswe.south =presetarray[i].nswe.south;
			options->nswe.west = presetarray[i].nswe.west;
			options->nswe.east = presetarray[i].nswe.east;
		}
	}

	return 1;
}

char * SuggestAreaFromNSWE(NSWE* viewport, PRESET * presetarray, int numberofpresets)
{

//	PRESET preset[48];
	//numberofpresets = 48;
	int i;
	int besti;
	double areaintersection, areaviewport, areapreset;
	double score, bestscore;
	NSWE intersection;

	bestscore=0;
	besti=0;
	areaviewport=AreaOfNSWE(viewport);

	for (i=0; i<numberofpresets; i++)	{
		IntersectionOfNSWEs(&intersection, viewport, &presetarray[i].nswe);
		areapreset = AreaOfNSWE(&presetarray[i].nswe);
		areaintersection = AreaOfNSWE(&intersection);
		score = min(areaintersection/areapreset, areaintersection/areaviewport);

		if (score>bestscore)	{
			bestscore = score;
			besti = i;
		}
	}
	return presetarray[besti].name;
}

void IntersectionOfNSWEs(NSWE *output, NSWE *d1, NSWE *d2)
{
	output->north = min(d1->north, d2->north);
	output->south = max(d1->south, d2->south);

	output->west = max(d1->west, d2->west);
	output->east = min(d1->east, d2->east);

	if ((output->south > output->north) || (output->west > output->east))	{
		output->north=output->south=output->west=output->east = 0;
	}

	return;
}

double AreaOfNSWE(NSWE *nswe)
{
	double area;
	area = (nswe->north - nswe->south) * (nswe->east - nswe->west);

	return area;
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
	lat1*=PI/180.0;
	long1*=PI/180.0;
	lat2*=PI/180.0;
	long2*=PI/180.0;

	double deltalat=lat2-lat1;
	double deltalong = long2-long1;
	double meanlat=(lat1+lat2)/2;
	//double a = PI/2 - lat1;
	//double b = PI/2 - lat2;
	//double u = a * a + b * b;

	//double v = -2 * a * b * cos(long2 - long1);
	//double c = sqrt(abs(u + v));

	double c=sqrt( (deltalat*deltalat) + ((cos(meanlat)*deltalong)*(cos(meanlat)*deltalong)) );
	//printf("%f ",deltalong);

	return EARTH_MEAN_RADIUS_KM*1000 * c;
}


double MetersApartHaversine(double lat1, double long1, double lat2, double long2)
{
	//convert to radians
	lat1*=PI/180.0;
	long1*=PI/180.0;
	lat2*=PI/180.0;
	long2*=PI/180.0;

    double deltalat = abs(lat1 - lat2);
    double deltalong = abs(long1 - long2);

    double a = pow(sin(deltalat / 2), 2) + cos(lat1) * cos(lat2) * pow(sin(deltalong / 2), 2);

    double dsigma = 2 * asin(sqrt(a));

    return EARTH_MEAN_RADIUS_KM*1000 * dsigma;
}


int CopyNSWE(NSWE *dest, NSWE *src)
{
	//this just does n=n, e=e, etc.
	memcpy(dest, src, sizeof(*dest));
	return 0;
}

int DeleteRegion(WORLDREGION * regionToDelete, WORLDREGION ** pRegionHead)
{
	WORLDREGION * prev;
	WORLDREGION * next;
	if ((regionToDelete->next) && (regionToDelete->prev))	{	//if it's a middle node
		prev = regionToDelete->prev;
		next = regionToDelete->next;

		prev->next = next;
		next->prev = prev;
		free(regionToDelete);
		return 0;
	}
	return 0;
}

int DeleteRegionByIndex(WORLDREGION ** regionHead, int index) 	//returns 1 if the last in the list
{
	WORLDREGION * cur;
	int i;

	cur = *regionHead;
	i=0;
	while (cur)	{
		if (i == index)	{	//we've found the one to delete
			if ((cur->next) && (cur->prev))	{
				cur->next->prev = cur->prev;
				cur->prev->next = cur->next;
				free(cur);
				return 0;
			}
			else if	(cur->prev)	{	//it's the end one
				cur->prev->next = cur->next;
				free(cur);
				return 1;
			}
			else if	(cur->next)	{	//if not the above, it must be the first
				cur->next->prev = NULL;
				*regionHead = cur->next;
				free(cur);
				return 0;
			}
			else {	//it's the first, the last and the only
				*regionHead = NULL;
				free(cur);
			}
		}
		cur = cur->next;
		i++;
	}
	return 0;
}

WORLDREGION * CreateRegionAfter(WORLDREGION * parentRegion, NSWE *nswe, char * title, int type, COLOUR *c, WORLDREGION ** pRegionHead)
{
	WORLDREGION *outputRegion;

	outputRegion = malloc(sizeof(WORLDREGION));
	outputRegion->next=NULL;
	outputRegion->prev=parentRegion;
	CopyNSWE(&outputRegion->nswe, nswe);
	outputRegion->baseColour.R=c->R;
	outputRegion->baseColour.G=c->G;
	outputRegion->baseColour.B=c->B;
	outputRegion->baseColour.A=c->A;

	//Set the title - NOT STRING SAFE
	if (title)	{
		outputRegion->title = malloc(strlen(title)+1);
		strcpy(outputRegion->title, title);
	}
	else	{
		outputRegion->title = NULL;
	}

	//set the type
	outputRegion->type = type;

	//set this as the next region in the list
	if (parentRegion)	{
		parentRegion->next = outputRegion;
	}

	return outputRegion;

}

STAY * CreateStayListFromNSWE(NSWE * nswe, LOCATIONHISTORY *lh)
{
	STAY * stay;
	STAY * oldstay;
	STAY * firststay;

	LOCATION * loc;
	WORLDCOORD coord;
	int inregion;

	stay=NULL;
	oldstay=NULL;
	firststay=NULL;

	loc=lh->first;
	inregion = 0;

//	FILE * f;
//	f= fopen("output2.txt", "w");

	while (loc)	{
		coord.latitude = loc->latitude;
		coord.longitude = loc->longitude;

		if (CoordInNSWE(&coord, nswe))	{
			if (!inregion)	{	//we've entered into the region (from outside)
				stay = malloc(sizeof(STAY));
				if (!firststay)	firststay = stay;
				stay->next=NULL;
				stay->arrivetime = loc->timestampS;

				if (oldstay)	oldstay->next=stay;
				oldstay=stay;
				inregion = 1;
			}
		}
		else	{	//if the coordinate isn't in the NSWE
			if (inregion)	{
				stay->leavetime = loc->timestampS;


				inregion=0;
			}
		}


		loc=loc->next;
	}

	//fclose(f);
	return firststay;
}

long SecondsInStay(STAY *stay, long starttime, long endtime)
{
	int staylength;

	staylength = 0;

	while (stay)	{
		if ((stay->leavetime > starttime) && (stay->arrivetime < endtime))	{
			staylength += min(stay->leavetime, endtime) - max(stay->arrivetime, starttime);
		//fprintf(stdout, "\nst%i et%i at%i lt%i", starttime, endtime, stay->arrivetime, stay->leavetime);
			//fprintf(stdout, " added %is", staylength);
		}
		if (stay->leavetime >= endtime)	{
			return staylength;	//this should work if the linked list is in order, and it would speed things up significantly

		}

		stay=stay->next;
	}
	return staylength;
}

int ExportTimeInNSWE(NSWE *nswe, long starttime, long endtime, long interval, LOCATIONHISTORY *lh)
{
	STAY * firststay;
	STAY * stay;
	long s;

	firststay = CreateStayListFromNSWE(nswe,lh);

	FILE * f;
	f= fopen("output.txt", "w");

	for (long l=starttime; l<endtime; l+=interval)	{
		stay=firststay;
		s= SecondsInStay(stay, l,l+interval);

		fprintf(f, "%i %i\n",l,s);
	}
	fclose(f);
	return 0;
}

int FreeLinkedListOfStays(STAY * stay)
{
	STAY * nextstay;
	nextstay=stay;

	while (stay)	{
		nextstay=stay->next;
		free(stay);
		stay=nextstay;
	}
	return 0;
}


TRIP * GetLinkedListOfTrips(WORLDREGION * regions, LOCATIONHISTORY *lh)
{
	LOCATION *loc;
	WORLDCOORD coord;

	int dir;

	TRIP * trip;
	TRIP * oldtrip;
	TRIP * firsttrip;

	WORLDREGION * firstRegion;
	WORLDREGION * reg;

	long leavetime;

	leavetime=0;
	trip=NULL;
	oldtrip=NULL;
	firsttrip=NULL;

	firstRegion=regions;

	dir = 0;
	loc=lh->first;
	while (loc)	{
		coord.latitude = loc->latitude;
		coord.longitude = loc->longitude;

		//go through all the regions.
		reg = firstRegion;
		while (reg)	{
			if (CoordInNSWE(&coord, &reg->nswe))	{
				if (reg->type == REGIONTYPE_EXCLUSION)	{
					dir = 0;
				}
				else if (reg->type == REGIONTYPE_HOME)	{
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
				else if (reg->type == REGIONTYPE_AWAY)	{
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
			}
			reg = reg->next;
		}


		loc=loc->next;
	}

	return firsttrip;
}





int ExportTripData(TRIP * firsttrip, char * filename)
{
	TRIP * trip;
	FILE *f;
	struct tm leavetime;
	struct tm arrivetime;

	f=fopen(filename, "w");
	if (!f) return 0;
	fprintf(f, "leavetime,arrivetime,seconds, direction,leavingyear,month,dayofmonth,dayofweek,hour,minute\n");

	trip=firsttrip;
	while (trip)	{
		if (trip->direction!=0)	{
			localtime_s(&trip->leavetime, &leavetime);
			localtime_s(&trip->arrivetime, &arrivetime);

			fprintf(f, "%i,%i,%i,%i, %i,%i,%i,%i,%i,%i, %i,%i,%i,%i,%i,%i,%i\n", trip->leavetime, trip->arrivetime, trip->leavetime - trip->arrivetime, trip->direction,
				leavetime.tm_year+1900,leavetime.tm_mon+1,leavetime.tm_mday,leavetime.tm_wday,leavetime.tm_hour,leavetime.tm_min,
				arrivetime.tm_year+1900,arrivetime.tm_mon+1,arrivetime.tm_mday,arrivetime.tm_wday,arrivetime.tm_hour,arrivetime.tm_min,arrivetime.tm_isdst);
		}
		trip=trip->next;
	}
	fclose(f);

	return 0;

}

int FreeLinkedListOfTrips(TRIP * trip)
{
	TRIP * nexttrip;
	nexttrip=trip;

	while (trip)	{
		nexttrip=trip->next;
		free(trip);
		trip=nexttrip;
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

void GraphLine(BM *bm, COLOUR *cBackground, double minx, double miny, double maxx, double maxy, COLOUR *cDataColour, int numberofpoints, double *xarray, double *yarray)
{
	int x,y;	//used in loops
	int i;

	double rangex,rangey;
	double xfactor,yfactor;
	int xtranslate,ytranslate;

	if (cBackground)	{
		for (x=0;x<bm->width; x++)	{
			for (y=0;y<bm->height; y++)	{
				bitmapPixelSet(bm, x,y, cBackground);
			}
		}
	}

	//if there's no points, no point continuing
	if (numberofpoints<1) {
		return;
	}


//	printf("\nBefore: xmin %f,ymin %f, xmax %f, ymax %f, rangex %f",minx,miny,maxx,maxy,rangex);
	//find the bounds of x
	if (minx-maxx<1)	{	//if they're both zero, or otherwise the same, it'll be automatic
		printf("\nFinding min and max x");
		minx= 1000000000000;
		maxx=-1000000000000;
		for (i=0;i<numberofpoints; i++)	{
			if (isfinite(xarray[i]))	{
				if (minx>xarray[i]) minx=xarray[i];
				if (maxx<xarray[i]) maxx=xarray[i];
			}
		}
	}

	if (miny-maxy<1)	{	//if they're both zero, or otherwise the same, it'll be automatic
		printf("\nFinding min and max y");
		miny= 1000000000000;
		maxy=-1000000000000;
		for (i=0;i<numberofpoints; i++)	{
			if (isfinite(yarray[i]))	{
				if (miny>yarray[i]) miny=yarray[i];
				if (maxy<yarray[i]) maxy=yarray[i];
			}
		}
	}

	rangex=maxx-minx;
	rangey=maxy-miny;

	xfactor=bm->width/rangex;
	yfactor=bm->height/rangey;

	//translation occurs before scaling
	xtranslate=-minx;
	ytranslate=-miny;

	printf("\nxmin %f,ymin %f, xmax %f, ymax %f, rangex %f",minx,miny,maxx,maxy,rangex);

	//Plot the points
	for (i=0;i<numberofpoints; i++)	{
		x=(xarray[i]+xtranslate)*xfactor;
		y=bm->height-1 - (yarray[i]+ytranslate)*yfactor;

		bitmapPixelSet(bm, x, y, cDataColour);
	//	printf("b%i %i %i, x,y:%f,%f",i,x,y,xarray[i],yarray[i]);
	}




	return;
}

//This will plot an axis (if required) then a list of x and y values
void GraphScatter(BM *bm, COLOUR *cBackground, double minx, double miny, double maxx, double maxy, double xmajorunit, double ymajorunit,\
	 COLOUR *cAxisAndLabels, char * xaxislabel, char * yaxislabel, void(*xlabelfn)(double, char *), void(*ylabelfn)(double, char *),\
	 COLOUR *cDataColour, int widthofpoint, int numberofpoints, double *xarray, double *yarray)
{
	int x,y;
	int i;

	double realx;
	double realy;
	double nextmajorunit;
	int plotmajorunit;

	//for the axis
	int margin;
	int xstart, ystart, xend, yend;
	int xlen, ylen;

	int middleofpoint;

	if (cBackground)	{
		for (x=0;x<bm->width; x++)	{
			for (y=0;y<bm->height; y++)	{
				bitmapPixelSet(bm, x,y, cBackground);
			}
		}
	}

	if ((!cAxisAndLabels) && (!cDataColour))	{	//if we just wanted a background
		return;
	}

	margin = (bm->width + bm->height)/2/10; //one tenth of the average

	xstart = margin;
	xend = bm->width - xstart;
	xlen = xend-xstart;

	ystart = bm->height - margin;	//on paper we start at the bottom
	yend = bm->height - ystart;
	ylen = yend-ystart;

	if (cAxisAndLabels)	{
		RASTERFONT rf;
		char szLabel[256];
		LoadRasterFont(&rf);
		nextmajorunit=ceil(minx/xmajorunit)*xmajorunit;
		plotmajorunit = xlen*(nextmajorunit-minx)/(maxx-minx)+xstart;

		for (x=xstart; x<=xend;x++)	{		//x axis
			bitmapPixelSet(bm, x,ystart, cAxisAndLabels);
			if (x>=plotmajorunit)	{
				bitmapPixelSet(bm, x,ystart+1, cAxisAndLabels);
				bitmapPixelSet(bm, x,ystart+2, cAxisAndLabels);
				bitmapPixelSet(bm, x,ystart+3, cAxisAndLabels);
				bitmapPixelSet(bm, x,ystart+4, cAxisAndLabels);

				if (xlabelfn!=NULL)	{
					xlabelfn(nextmajorunit, szLabel);
				}
				else 	{
					sprintf(szLabel, "%.0f", nextmajorunit);
				}

				bitmapText(bm,&rf, x-3,ystart+10, szLabel, cAxisAndLabels);

				//messy but works
				realx = (double)(x-margin)/(double)xlen * (maxx-minx)+minx;
				nextmajorunit=ceil((realx+xmajorunit)/xmajorunit)*xmajorunit;
				plotmajorunit = xlen*(nextmajorunit-minx)/(maxx-minx)+xstart;

			}
		}

		nextmajorunit=ceil(miny/ymajorunit)*ymajorunit;
		plotmajorunit = ylen*(nextmajorunit-miny)/(maxy-miny)+ystart;
//		printf("\nmargin%i. ylen%i, maxy%f,miny%f, yend %i, ystart %i, ymu %f, nextmajorunit: %f, pmu: %i",margin, ylen, maxy,miny,yend, ystart, ymajorunit, nextmajorunit, plotmajorunit);
		for (y=ystart; y>=yend;y--)	{	//go backward
			bitmapPixelSet(bm, xstart,y, cAxisAndLabels);
			if (y<=plotmajorunit)	{
				bitmapPixelSet(bm, xstart-1, y, cAxisAndLabels);
				bitmapPixelSet(bm, xstart-2, y, cAxisAndLabels);
				bitmapPixelSet(bm, xstart-3, y, cAxisAndLabels);
				bitmapPixelSet(bm, xstart-4, y, cAxisAndLabels);


				if (ylabelfn!=NULL)	{
					ylabelfn(nextmajorunit, szLabel);
				}
				else 	{
					sprintf(szLabel, "%.0f", nextmajorunit);
				}

				bitmapText(bm,&rf, xstart-fontGetStringWidth(&rf,szLabel)-5,y-rf.approxheight/2-1, szLabel, cAxisAndLabels);


								//messy but works
				realy = (double)(y-ystart)/(double)ylen * (maxy-miny)+miny;
				nextmajorunit=((long)((realy+ymajorunit)/ymajorunit))*ymajorunit;
				plotmajorunit = ylen*(nextmajorunit-miny)/(maxy-miny)+ystart;
//				printf("\n%i %f.ymu %f, nextmajorunit: %f, pmu: %i",y, realy, ymajorunit, nextmajorunit, plotmajorunit);


			}

		}

		DestroyRasterFont(&rf);
	}


	if (!cDataColour)	{
		return;
	}
	//Now to plot the data

	middleofpoint = (widthofpoint+1)/2;
	for (i=0;i<numberofpoints;i++)	{

		x=xlen*(xarray[i]-minx)/(maxx-minx)+xstart-middleofpoint+1;
		y=ylen*(yarray[i]-miny)/(maxy-miny)+ystart-middleofpoint+1;

		//bitmapSquare(bm, x,y,x+widthofpoint-1, y+widthofpoint-1, cDataColour, cDataColour);
		bitmapFilledCircle(bm, x, y, widthofpoint, cDataColour);

//		bitmapSquare(bm, x,y,x+widthofpoint-1, ystart, cDataColour, cDataColour);	//bar
	}


	return;
}

void labelfnShortDayOfWeekFromSeconds(double seconds, char * outputString)	//actually seconds from midnight
{
	struct tm time;
	unsigned long s;
	time_t sundaymidnight;

	s=seconds;	//this is seconds since midnight Sunday

	//make a midnight sunday that doesn't cross feb 29 or daylight savings
	time.tm_sec=0;	time.tm_min=0;	time.tm_hour=0;
	time.tm_mday=7;	time.tm_mon=5; time.tm_year=81;
	time.tm_isdst=0;
	sundaymidnight = mktime(&time);

	//add to seconds since sunday.
	s+=sundaymidnight;

	localtime_s(&s, &time);

	strftime (outputString,80,"%A",&time);
	return;
}

void labelfnTimeOfDayFromSeconds(double seconds, char * outputString)
{
	unsigned long s;

	s=seconds;	//for this in particular we measure seconds from midnight

	int h;
	int m;

	h=s/3600;
	m=(s-h*3600)/60;
	sprintf(outputString,"%ih",h);
	return;
}


void labelfnMinutesFromSeconds(double seconds, char * outputString)
{
	int m;

	m = seconds/60;
	sprintf(outputString, "%i", m);
	return;
}
