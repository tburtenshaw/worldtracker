#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include <time.h>

#include "worldtrackerres.h"
#include "mytrips.h"
#include "lodepng.h"

#define WT_WM_RECALCBITMAP WM_APP+0	//Redraw the map from the loaded list of locations - this will do it as soon as message received
#define WT_WM_QUEUERECALC WM_APP+1	//Signal that the map needs to be replotted (essentially this starts a timer) so we don't waste time
#define WT_WM_SIGNALMOUSEWHEEL WM_APP+2	//We shouldn't send WM_MOUSEWHEEL between functions, but send this to down propagate to child window if required

#define IDT_PREVIEWTIMER 1

#define OVERVIEW_WIDTH 360
#define OVERVIEW_HEIGHT 180
#define MARGIN 12
#define TEXT_HEIGHT 20
#define TEXT_WIDTH_QUARTER (OVERVIEW_WIDTH-3*MARGIN)/4
#define TEXT_WIDTH_THIRD (OVERVIEW_WIDTH-2*MARGIN)/3
#define TEXT_WIDTH_HALF (OVERVIEW_WIDTH-MARGIN)/2
#define TEXT_WIDTH_QSHORT TEXT_WIDTH_QUARTER-MARGIN
#define TEXT_WIDTH_QLONG TEXT_WIDTH_QUARTER+MARGIN
#define TEXT_WIDTH_THREEQUARTERS (OVERVIEW_WIDTH-TEXT_WIDTH_QUARTER-MARGIN)


#define ID_EDITNORTH 1001
#define ID_EDITSOUTH 1002
#define ID_EDITWEST 1003
#define ID_EDITEAST 1004
#define ID_EDITPRESET 1005
#define ID_EDITEXPORTWIDTH 1006
#define ID_EDITEXPORTHEIGHT 1007
#define ID_EDITDATEFROM 1008
#define ID_EDITDATETO 1009
#define ID_EDITTHICKNESS 1010
#define ID_EDITCOLOURBY 1011


//these don't really need to be the bearing, but I thought it would be easier
#define D_NORTH 0
#define D_SOUTH 180
#define D_WEST 270
#define D_EAST 90

//what the aspect ratio is locked to
//#define LOCK_AR_UNDECLARED 0
//#define LOCK_AR_NSWE 1
//#define LOCK_AR_EXPORTSIZE 2
//#define LOCK_AR_WINDOW 3

typedef struct sMovebar MOVEBARINFO;
typedef struct sStretch STRETCH;

struct sMovebar	{
	int direction;
	float position;
	COLORREF colour;
};

struct sStretch	{
	BOOL useStretch;	//should paint use this to stretchblt
	int   nXOriginDest;	//these are set by mousewheel zoom function
	int   nYOriginDest; //to mimic the look of zooming
	int   nWidthDest;	//without having to recalculate fully
	int   nHeightDest;
	int   nXOriginSrc;
	int   nYOriginSrc;
	int   nWidthSrc;
	int   nHeightSrc;
};


LOCATIONHISTORY locationHistory;
HINSTANCE hInst;		// Instance handle
HWND hWndMain;		//Main window handle
HWND hwndOverview;	//The overview is also used to hold the positions to be drawn and exported
HWND hwndPreview;

//The bars that can be moved to set the viewport
HWND hwndOverviewMovebarNorth;
HWND hwndOverviewMovebarSouth;
HWND hwndOverviewMovebarEast;
HWND hwndOverviewMovebarWest;
//each of these windows has the same windowproc, so the WindowLongPtr is set to some info
MOVEBARINFO mbiNorth;
MOVEBARINFO mbiSouth;
MOVEBARINFO mbiEast;
MOVEBARINFO mbiWest;

HWND  hWndStatusbar;

//The dialog hwnds
HWND hwndStaticFilename;

HWND hwndStaticNorth;
HWND hwndStaticSouth;
HWND hwndStaticWest;
HWND hwndStaticEast;

HWND hwndEditNorth;
HWND hwndEditSouth;
HWND hwndEditWest;
HWND hwndEditEast;

HWND hwndStaticPreset;
HWND hwndEditPreset;

HWND hwndStaticExportHeading;
HWND hwndStaticExportWidth;
HWND hwndEditExportWidth;
HWND hwndStaticExportHeight;
HWND hwndEditExportHeight;

HWND hwndEditDateTo;
HWND hwndEditDateFrom;
HWND hwndEditThickness;

HWND hwndDateSlider;


HBITMAP hbmOverview;	//this bitmap is generated when the overview is updated.
HBITMAP hbmPreview;

STRETCH stretchPreview;

BM overviewBM;
BM previewBM;


//Mouse dragging
POINT overviewOriginalPoint;
POINT previewOriginalPoint;

NSWE overviewOriginalNSWE;
NSWE previewOriginalNSWE;

BOOL mouseDragMovebar;
BOOL mouseDragOverview;
BOOL mouseDragPreview;


//Options are still based on the command line program
OPTIONS optionsOverview;
OPTIONS optionsPreview;
OPTIONS optionsExport;

//Multithread stuff
//HANDLE hAccessLocationsMutex;
CRITICAL_SECTION critAccessLocations;	//if we are using locations (perhaps I should distinguish between reading at writing)
long hbmQueueSize=0;	//this is increased when a thread is created, so the thread can check that nothing was created after it

DWORD WINAPI LoadKMLThread(void *LoadKMLThreadData);
DWORD WINAPI ThreadSaveKML(OPTIONS *info);
DWORD WINAPI ThreadSetHBitmap(long queuechit);	//we actually only take someone from the back of the queue

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK PreviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK OverviewMovebarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK DateSliderWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

int PreviewWindowFitToAspectRatio(HWND hwnd, int mainheight, int mainwidth, double aspectratio);	//aspect ratio here is width/height, if 0, it doesn't fits to the screen
double PreviewFixParamsToLock(int lockedPart, OPTIONS * o);


HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh);
HBITMAP MakeHBitmapPreview(HDC hdc, LOCATIONHISTORY * lh, long queuechit);

int PaintOverview(HWND hwnd, LOCATIONHISTORY * lh);
int PaintPreview(HWND hwnd, LOCATIONHISTORY * lh);

int PaintDateSlider(HWND hwnd, LOCATIONHISTORY * lh, OPTIONS *o);

int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh);
int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

int HandlePreviewMousewheel(HWND hwnd, WPARAM wParam, LPARAM lParam);
int HandlePreviewLbutton(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int UpdateEditboxesFromOptions(OPTIONS * o);
int UpdateBarsFromNSWE(NSWE * d);
int UpdateExportAspectRatioFromOptions(OPTIONS * o, int forceHeight);



void UpdateStatusBar(LPSTR lpszStatusString, WORD partNumber, WORD displayFlags)
{
    SendMessage(hWndStatusbar, SB_SETTEXT, partNumber | displayFlags, (LPARAM)lpszStatusString);
}


LRESULT MsgMenuSelect(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    static char szBuffer[256];
    UINT   nStringID = 0;
    UINT   fuFlags = GET_WM_MENUSELECT_FLAGS(wparam, lparam) & 0xffff;
    UINT   uCmd    = GET_WM_MENUSELECT_CMD(wparam, lparam);
    HMENU  hMenu   = GET_WM_MENUSELECT_HMENU(wparam, lparam);

    szBuffer[0] = 0;                            // First reset the buffer
    if (fuFlags == 0xffff && hMenu == NULL)     // Menu has been closed
        nStringID = 0;

    else if (fuFlags & MFT_SEPARATOR)           // Ignore separators
        nStringID = 0;

    else if (fuFlags & MF_POPUP)                // Popup menu
    {
        if (fuFlags & MF_SYSMENU)               // System menu
            nStringID = IDS_SYSMENU;
        else
            // Get string ID for popup menu from idPopup array.
            nStringID = 0;
    }  // for MF_POPUP
    else                                        // Must be a command item
        nStringID = uCmd;                       // String ID == Command ID

    // Load the string if we have an ID
    if (0 != nStringID)
        LoadString(hInst, nStringID, szBuffer, sizeof(szBuffer));
    // Finally... send the string to the status bar
    UpdateStatusBar(szBuffer, 0, 0);
    return 0;
}

void InitializeStatusBar(HWND hwndParent,int nrOfParts)
{
    const int cSpaceInBetween = 8;
    int   ptArray[40];   // Array defining the number of parts/sections
    RECT  rect;
    HDC   hDC;

   /* * Fill in the ptArray...  */

    hDC = GetDC(hwndParent);
    GetClientRect(hwndParent, &rect);

    ptArray[nrOfParts-1] = rect.right;
    //---TODO--- Add code to calculate the size of each part of the status
    // bar here.

    ReleaseDC(hwndParent, hDC);
    //SendMessage(hWndStatusbar, SB_SETPARTS, nrOfParts, LPARAM)(LPINT)ptArray);
    UpdateStatusBar("Ready", 0, 0);
    //---TODO--- Add code to update all fields of the status bar here.
    // As an example, look at the calls commented out below.

//    UpdateStatusBar("Cursor Pos:", 1, SBT_POPOUT);
//    UpdateStatusBar("Time:", 3, SBT_POPOUT);
}


static BOOL CreateSBar(HWND hwndParent,char *initialText,int nrOfParts)
{
    hWndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER|SBARS_SIZEGRIP,
                                       initialText,
                                       hwndParent,
                                       IDM_STATUSBAR);
    if(hWndStatusbar)
    {
        InitializeStatusBar(hwndParent,nrOfParts);
        return TRUE;
    }

    return FALSE;
}

int GetFileName(char *buffer,int buflen)
{
	char tmpfilter[41];
	int i = 0;
	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = buflen;
	ofn.lpstrTitle = "Open";
	ofn.nFilterIndex = 2;
	ofn.lpstrDefExt = "json";
	strcpy(buffer,"*.json");
	strcpy(tmpfilter,"All files,*.*,JSON Files,*.json");
	while(tmpfilter[i]) {
		if (tmpfilter[i] == ',')
			tmpfilter[i] = 0;
		i++;
	}
	tmpfilter[i++] = 0; tmpfilter[i++] = 0;
	ofn.Flags = 539678;
	ofn.lpstrFilter = tmpfilter;
	return GetOpenFileName(&ofn);

}


static BOOL InitApplication(void)
{
	WNDCLASS wc;

	//Set locationHistory to NULL
	memset(&locationHistory,0,sizeof(locationHistory));
	memset(&previewBM,0,sizeof(previewBM));

	memset(&optionsPreview,0,sizeof(optionsPreview));
	memset(&optionsOverview,0,sizeof(optionsPreview));

	optionsPreview.nswe.north=90;
	optionsPreview.nswe.south=-90;
	optionsPreview.nswe.west=-180;
	optionsPreview.nswe.east=180;
	optionsPreview.thickness=1;

	//Make the Window Classes
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "worldtrackerWndClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	if (!RegisterClass(&wc))
		return 0;

	//The Overview window class
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = (WNDPROC)OverviewWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.lpszClassName = "OverviewClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//The Preview window class
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)PreviewWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.lpszClassName = "PreviewClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//The class for movable bar to select position in Overview
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)OverviewMovebarWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "OverviewMovebarClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_SIZEWE);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;

	//Date slider
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)DateSliderWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
	wc.cbWndExtra = 4;
	wc.lpszClassName = "DateSliderClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_SIZEWE);
	wc.hIcon = NULL;
	if (!RegisterClass(&wc))
		return 0;


	return 1;
}

BOOL _stdcall AboutDlg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CLOSE:
			EndDialog(hwnd,0);
			return 1;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hwnd,1);
					return 1;
			}
			break;
	}
	return 0;
}



void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{	char JSONfilename[MAX_PATH];

	switch(id) {
		case IDM_ABOUT:
			DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUT),
				hWndMain,AboutDlg);
			break;

		case IDM_OPEN:
			if (GetFileName(JSONfilename,sizeof(JSONfilename))==0)
				return;

//			hAccessLocationsMutex = CreateMutex(NULL, FALSE, "accesslocations");
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadKMLThread, &JSONfilename ,0,NULL);

			UpdateEditboxesFromOptions(&optionsPreview);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateExportAspectRatioFromOptions(&optionsPreview, 0);

		break;

		case IDM_SAVE:
			ExportKMLDialogAndComplete(hwnd, &optionsPreview, &locationHistory);
		break;

		case IDM_EXIT:
		PostMessage(hwnd,WM_CLOSE,0,0);
		break;

		case ID_EDITEXPORTHEIGHT:
		case ID_EDITEXPORTWIDTH:
		case ID_EDITNORTH:
		case ID_EDITSOUTH:
		case ID_EDITEAST:
		case ID_EDITWEST:
		case ID_EDITPRESET:
		case ID_EDITTHICKNESS:
			HandleEditControls(hwnd, id, hwndCtl, codeNotify);
		break;
	}
}

//This is the thread that loads the file
DWORD WINAPI LoadKMLThread(void *JSONfilename)
{
//	UpdateStatusBar("Loading file...", 0, 0);

    EnterCriticalSection(&critAccessLocations);

	FreeLocations(&locationHistory);	//first free any locations
	LoadLocations(&locationHistory, JSONfilename);

	LeaveCriticalSection(&critAccessLocations);

	//Once loaded, tell the windows they can update
//    UpdateStatusBar("Ready", 0, 0);
	SendMessage(hwndOverview, WT_WM_RECALCBITMAP, 0,0);
	SendMessage(hwndPreview, WT_WM_RECALCBITMAP, 0,0);
	return 0;
}


int UpdateEditboxesFromOptions(OPTIONS * o)
{
	char buffer[256];
	sprintf(buffer,"%s", o->jsonfilenamefinal);
	SetWindowText(hwndStaticFilename, buffer);

	sprintf(buffer,"%f", o->nswe.north);
	SetWindowText(hwndEditNorth, buffer);
	sprintf(buffer,"%f", o->nswe.south);
	SetWindowText(hwndEditSouth, buffer);
	sprintf(buffer,"%f", o->nswe.west);
	SetWindowText(hwndEditWest, buffer);
	sprintf(buffer,"%f", o->nswe.east);
	SetWindowText(hwndEditEast, buffer);

	return 0;
}

int UpdateBarsFromNSWE(NSWE * d)
{
	MoveWindow(hwndOverviewMovebarWest, 180 + d->west , 0, 3,180,TRUE);
	MoveWindow(hwndOverviewMovebarEast, 180 + d->east , 0, 3,180,TRUE);
	MoveWindow(hwndOverviewMovebarNorth, 0, 90 - d->north, 360,3,TRUE);
	MoveWindow(hwndOverviewMovebarSouth, 0, 90 - d->south, 360,3,TRUE);

	InvalidateRect(hwndOverview,0,FALSE);	//this doesn't always work
	return 0;
}

int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	char szText[128];
	double value;	//to hold the textbox value
	int needsRedraw;

//	printf("%i\t", codeNotify);

	if (codeNotify==EN_CHANGE)	{
	//If not in focus we shouldn't do anything with the change
	if (hwndCtl!=GetFocus())
		return 0;

		needsRedraw=0;
		SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
		value=atof(szText);

		switch (id)	{
		case ID_EDITNORTH:
			if ((value >= -90) && (value<=90))
				if (optionsPreview.nswe.north!=value)	{
					optionsPreview.nswe.north=value;
					needsRedraw = 1;
					printf("N");
				}
			break;
		case ID_EDITSOUTH:
			if ((value >= -90) && (value<=90))
				if (optionsPreview.nswe.south!=value)	{
					optionsPreview.nswe.south=value;
					needsRedraw = 1;
					printf("S");
				}
			break;
		case ID_EDITWEST:
			if ((value >= -180) && (value<=180))	{
				if (optionsPreview.nswe.west!=value)	{
					optionsPreview.nswe.west=value;
					needsRedraw = 1;
					printf("W");
				}
			}
		break;
		case ID_EDITEAST:
			if ((value >= -180) && (value<=180))
				if (optionsPreview.nswe.east != value)	{
					optionsPreview.nswe.east=value;
					needsRedraw = 1;
					printf("E");
				}
			break;

		case ID_EDITEXPORTHEIGHT:
			UpdateExportAspectRatioFromOptions(&optionsPreview, 1);
			break;
		case ID_EDITEXPORTWIDTH:
			UpdateExportAspectRatioFromOptions(&optionsPreview, 0);
			break;

		case ID_EDITTHICKNESS:
			optionsPreview.thickness=value;
			needsRedraw=1;
			break;

		}

		if (needsRedraw)	{
			printf("redraw from edit\r\n");
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			UpdateBarsFromNSWE(&optionsPreview.nswe);
		}

	}
	else if (codeNotify== EN_KILLFOCUS)	{
		switch (id)	{
			case ID_EDITPRESET:
				SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
				LoadPreset(&optionsPreview, szText);
				UpdateEditboxesFromOptions(&optionsPreview);
				UpdateBarsFromNSWE(&optionsPreview.nswe);
				UpdateExportAspectRatioFromOptions(&optionsPreview,0);
				InvalidateRect(hwndOverview, NULL, FALSE);
				SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
				break;
		}

	}


	return 0;
}


int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh)
{
	char filename[MAX_PATH];
	char editboxtext[128];
	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hInst;
	filename[0]=0;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Export to...";
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = "kml";
	ofn.Flags =OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = "Kml files (*.kml)\0*.kml\0\0";


	if (GetSaveFileName(&ofn)==0)
		return 0;

	printf("Filename:%s\r\n",filename);

	//Create export options
	memset(&optionsExport, 0, sizeof(optionsExport));

	optionsExport.kmlfilenameinput = filename;
	sprintf(optionsExport.kmlfilenamefinal, "%s", filename);
	sprintf(optionsExport.pngfilenamefinal, "%s.png", filename);

	optionsExport.pngfilenameinput = filename;

	SendMessage(hwndEditExportWidth, WM_GETTEXT, 128,(long)&editboxtext[0]);
	optionsExport.width=atol(&editboxtext[0]);
	SendMessage(hwndEditExportHeight, WM_GETTEXT, 128,(long)&editboxtext[0]);
	optionsExport.height=atol(&editboxtext[0]);
	//get the NSWE, thickness, dates and colour info from the options
	optionsExport.nswe.north=o->nswe.north;
	optionsExport.nswe.south=o->nswe.south;
	optionsExport.nswe.west=o->nswe.west;
	optionsExport.nswe.east=o->nswe.east;
	optionsExport.thickness = o->thickness;

	optionsExport.colourcycle = (60*60*24);
	optionsExport.totimestamp =-1;
	optionsExport.zoom=optionsExport.width/(optionsExport.nswe.east-optionsExport.nswe.west);
	optionsExport.alpha=200;	//default
	optionsExport.colourby = COLOUR_BY_TIME;

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadSaveKML, &optionsExport ,0,NULL);

	return 0;
}

DWORD WINAPI ThreadSaveKML(OPTIONS *options)
{
	BM exportBM;
	int error;

	EnterCriticalSection(&critAccessLocations);

	printf("bitmap init %i", options->width);
	bitmapInit(&exportBM, options, &locationHistory);
	DrawGrid(&exportBM);
	PlotPaths(&exportBM, &locationHistory, options);

	printf("png:%s, w:%i, h:%i size:%i\r\n",exportBM.options->pngfilenamefinal, exportBM.width, exportBM.height, exportBM.sizebitmap);
	error = lodepng_encode32_file(exportBM.options->pngfilenamefinal, exportBM.bitmap, exportBM.width, exportBM.height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));

	//Write KML file
	fprintf(stdout, "KML to %s. PNG to %s\r\n", exportBM.options->kmlfilenamefinal, exportBM.options->pngfilenamefinal);
	WriteKMLFile(&exportBM);
	bitmapDestroy(&exportBM);

	LeaveCriticalSection(&critAccessLocations);

	return 0;
}



HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh)
{
	HBITMAP bitmap;
	BITMAPINFO bi;
	BYTE * bits;
	int x;
	int y;
	COLOUR c;
	COLOUR d;	//window colour

	int result;

	if (hbmOverview!=NULL)	{
		DeleteObject(hbmOverview);
	}

	bitmap = LoadImage(hInst, MAKEINTRESOURCE(IDB_OVERVIEW), IMAGE_BITMAP, 0,0,LR_DEFAULTSIZE|LR_CREATEDIBSECTION);
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);

	if (lh->first==NULL)	return bitmap;

	result = GetDIBits(hdc, bitmap, 0, 180, NULL, &bi,DIB_RGB_COLORS);	//get the info

	printf("bi.height: %i\t", bi.bmiHeader.biHeight);
	printf("bi.bitcount: %i\t", bi.bmiHeader.biBitCount);
	printf("bi.sizeimage: %i\t\r\n", bi.bmiHeader.biSizeImage);


	bits=malloc(bi.bmiHeader.biSizeImage);

	result = GetDIBits(hdc, bitmap, 0, 180, bits, &bi, DIB_RGB_COLORS);	//get the bitmap location

	for (y=0;y<180;y++)	{
		for (x=0;x<360;x++)	{
			c= bitmapPixelGet(&overviewBM, x,y);
			if (c.A>0) {	//don't bother if there's not a visible pixel
				d.B=bits[x*3+(180-y-1)*360*3+0];
				d.G=bits[x*3+(180-y-1)*360*3+1];
				d.R=bits[x*3+(180-y-1)*360*3+2];

				mixColours(&d, &c);

				bits[x*3+(180-y-1)*360*3+0] = d.B;
				bits[x*3+(180-y-1)*360*3+1] = d.G;
				bits[x*3+(180-y-1)*360*3+2] = d.R;
			}
		}
	}


	result = SetDIBits(hdc,bitmap,0,180, bits,&bi,DIB_RGB_COLORS);
	free(&bits);


	return bitmap;
}

int PaintOverview(HWND hwnd, LOCATIONHISTORY * lh)
{
	HDC hdc;
	HDC memDC;
	PAINTSTRUCT ps;
	HGDIOBJ oldBitmap;

	hdc= BeginPaint(hwnd, &ps);
	memDC = CreateCompatibleDC(hdc);

	oldBitmap = SelectObject(memDC, hbmOverview);
	BitBlt(hdc,0, 0, 360, 180, memDC, 0, 0, SRCCOPY);
	SelectObject(memDC, oldBitmap);


	DeleteDC(hdc);
	EndPaint(hwnd, &ps);

	return 0;
}

LRESULT CALLBACK OverviewMovebarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	MOVEBARINFO * pMbi;
	POINT mousePoint;

	switch (msg) {
		case WM_SETCURSOR:
			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if ((pMbi->direction == D_NORTH)||(pMbi->direction == D_SOUTH))	{
				SetCursor(LoadCursor(NULL,IDC_SIZENS));
			}
			else	{
				SetCursor(LoadCursor(NULL,IDC_SIZEWE));
			}
			return 1;
			break;
		case WM_LBUTTONDOWN:
			SetFocus(hwnd);	//mainly to get the focus out of the edit boxes
			GetCursorPos(&mousePoint);
			mouseDragMovebar=TRUE;
			SetCapture(hwnd);
		break;
		case WM_MOUSEMOVE:
			if (mouseDragMovebar == FALSE)
				return 1;
			GetCursorPos(&mousePoint);
			ScreenToClient(GetParent(hwnd), &mousePoint);
			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

			if ((pMbi->direction == D_NORTH)||(pMbi->direction == D_SOUTH))	{
				if (mousePoint.y<0)	mousePoint.y=0;
				if (mousePoint.y>180)	mousePoint.y=180;
				MoveWindow(hwnd, 0, mousePoint.y, 360,3,TRUE);
			}


			if ((pMbi->direction == D_WEST)||(pMbi->direction == D_EAST))	{
				if (mousePoint.x<-1)	mousePoint.x=-1;
				if (mousePoint.x>359)	mousePoint.x=359;
				MoveWindow(hwnd, mousePoint.x, 0, 3,180,TRUE);
			}

		break;
		case WM_LBUTTONUP:
			if (mouseDragMovebar == FALSE)
				return 1;

			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			mouseDragMovebar=FALSE;
			ReleaseCapture();
			GetCursorPos(&mousePoint);
			ScreenToClient(GetParent(hwnd), &mousePoint);

			if ((pMbi->direction == D_NORTH)||(pMbi->direction == D_SOUTH))	{
				if (mousePoint.y<0)	mousePoint.y=0;
				if (mousePoint.y>180)	mousePoint.y=180;
				MoveWindow(hwnd, 0, mousePoint.y, 360,3,TRUE);
			}


			if ((pMbi->direction == D_WEST)||(pMbi->direction == D_EAST))	{
				if (mousePoint.x<-1)	mousePoint.x=-1;
				if (mousePoint.x>359)	mousePoint.x=359;
				MoveWindow(hwnd, mousePoint.x, 0, 3,180,TRUE);
			}

			if (pMbi->direction == D_WEST)	{
				optionsPreview.nswe.west = mousePoint.x-180;
			}
			if (pMbi->direction == D_EAST)	{
				optionsPreview.nswe.east = mousePoint.x-180;
			}
			if (pMbi->direction == D_NORTH)	{
				optionsPreview.nswe.north =90-mousePoint.y;
			}
			if (pMbi->direction == D_SOUTH)	{
				optionsPreview.nswe.south =90-mousePoint.y;
			}
			UpdateEditboxesFromOptions(&optionsPreview);
			UpdateExportAspectRatioFromOptions(&optionsPreview,0);

			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);

		break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}


	return 0;
}

LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	POINT mousePoint;

	switch (msg) {
		case WM_CREATE:
			hbmOverview = NULL;		//set to null, as it deletes the object if not
			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), &locationHistory);
			hwndOverviewMovebarWest = CreateWindow("OverviewMovebarClass", "West", WS_CHILD|WS_VISIBLE, 10,0,3,180,hwnd, NULL, hInst, NULL);
			mbiWest.direction = D_WEST;
			SetWindowLongPtr(hwndOverviewMovebarWest, GWL_USERDATA, (LONG)&mbiWest);

			hwndOverviewMovebarEast = CreateWindow("OverviewMovebarClass", "East", WS_CHILD|WS_VISIBLE, 350,0,3,180,hwnd, NULL, hInst, NULL);
			mbiEast.direction = D_EAST;
			SetWindowLongPtr(hwndOverviewMovebarEast, GWL_USERDATA, (LONG)&mbiEast);

			hwndOverviewMovebarNorth = CreateWindow("OverviewMovebarClass", "North", WS_CHILD|WS_VISIBLE, 0,10,360,3,hwnd, NULL, hInst, NULL);
			mbiNorth.direction = D_NORTH;
			SetWindowLongPtr(hwndOverviewMovebarNorth, GWL_USERDATA, (LONG)&mbiNorth);

			hwndOverviewMovebarSouth = CreateWindow("OverviewMovebarClass", "South", WS_CHILD|WS_VISIBLE, 0,20,360,3,hwnd, NULL, hInst, NULL);
			mbiSouth.direction = D_SOUTH;
			SetWindowLongPtr(hwndOverviewMovebarSouth, GWL_USERDATA, (LONG)&mbiSouth);

			UpdateBarsFromNSWE(&optionsPreview.nswe);
			break;

		case WT_WM_RECALCBITMAP:
			memset(&optionsOverview, 0, sizeof(optionsOverview));	//set all the option results to 0
			memset(&overviewBM, 0, sizeof(overviewBM));

			optionsOverview.height=180;optionsOverview.width=360;
			optionsOverview.thickness = 1;

			optionsOverview.colourcycle = (60*60*24);
			optionsOverview.totimestamp =-1;
			optionsOverview.zoom=optionsOverview.width/(optionsOverview.nswe.east-optionsOverview.nswe.west);
			optionsOverview.alpha=200;	//default
			optionsOverview.colourby = COLOUR_BY_TIME;

			optionsOverview.nswe.north=90;
			optionsOverview.nswe.south=-90;
			optionsOverview.nswe.west=-180;
			optionsOverview.nswe.east=180;


			//RationaliseOptions(&optionsOverview);

			bitmapInit(&overviewBM, &optionsOverview, &locationHistory);
			PlotPaths(&overviewBM, &locationHistory, &optionsOverview);
			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), &locationHistory);

			printf("recalc'd overview BM");

			InvalidateRect(hwnd, 0, 0);

			break;
		case WM_ERASEBKGND:	//to avoid flicker:(although shouldn't need if hbrush is null)
    		return 1;
		case WM_PAINT:
			PaintOverview(hwnd, &locationHistory);
			break;
		case WM_LBUTTONDOWN:
			SetFocus(hwnd);	//mainly to get the focus out of the edit boxes
			GetCursorPos(&overviewOriginalPoint);
			CopyNSWE(&overviewOriginalNSWE, &optionsPreview.nswe);
			//overviewOriginalNSWE.north = optionsOverview.north;
			//overviewOriginalNSWE.south = optionsOverview.south;
			//overviewOriginalNSWE.west = optionsOverview.west;
			//overviewOriginalNSWE.east = optionsOverview.east;

			mouseDragOverview=TRUE;
			SetCapture(hwnd);
			break;
		case WM_MOUSEMOVE:
			if (mouseDragOverview == FALSE)
				return 1;
			GetCursorPos(&mousePoint);
			optionsPreview.nswe.north=overviewOriginalNSWE.north-mousePoint.y+overviewOriginalPoint.y;
			optionsPreview.nswe.south=overviewOriginalNSWE.south-mousePoint.y+overviewOriginalPoint.y;
			optionsPreview.nswe.west=overviewOriginalNSWE.west+mousePoint.x-overviewOriginalPoint.x;
			optionsPreview.nswe.east=overviewOriginalNSWE.east+mousePoint.x-overviewOriginalPoint.x;
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateEditboxesFromOptions(&optionsPreview);
			UpdateExportAspectRatioFromOptions(&optionsPreview,0);
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);

			break;

		case WM_LBUTTONUP:
			if (mouseDragOverview == FALSE)
				return 1;
			mouseDragOverview=FALSE;
			ReleaseCapture();
			break;



		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

int HandlePreviewLbutton(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	double dpp;	//degrees per pixel
	POINT mousePoint;
	int mouseDeltaX, mouseDeltaY;

	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);

	switch (msg)	{
		case WM_LBUTTONDOWN:
			previewOriginalPoint.x=mousePoint.x;
			previewOriginalPoint.y=mousePoint.y;
			CopyNSWE(&previewOriginalNSWE, &optionsPreview.nswe);
			mouseDragPreview=TRUE;
			SetFocus(hwnd);
			SetCapture(hwnd);
		break;
		case WM_MOUSEMOVE:
			if (mouseDragPreview == FALSE)
				return 1;
			dpp = (optionsPreview.nswe.east - optionsPreview.nswe.west)/(optionsPreview.width);	//dpp aspect ratio should be 1, so only need to read x
			mouseDeltaX = mousePoint.x-previewOriginalPoint.x;
			mouseDeltaY = mousePoint.y-previewOriginalPoint.y;

			optionsPreview.nswe.east= previewOriginalNSWE.east - dpp*(mouseDeltaX);
			optionsPreview.nswe.west= previewOriginalNSWE.west - dpp*(mouseDeltaX);
			optionsPreview.nswe.north= previewOriginalNSWE.north + dpp*(mousePoint.y-previewOriginalPoint.y);
			optionsPreview.nswe.south= previewOriginalNSWE.south + dpp*(mousePoint.y-previewOriginalPoint.y);

			stretchPreview.nXOriginSrc=0;
			stretchPreview.nYOriginSrc=0;
			stretchPreview.nWidthSrc=optionsPreview.width;
			stretchPreview.nHeightSrc=optionsPreview.height;


			stretchPreview.nXOriginDest=0;
			stretchPreview.nYOriginDest=0;
			stretchPreview.nWidthDest=optionsPreview.width;
			stretchPreview.nHeightDest=optionsPreview.height;

			//now set the stretchblt info
			if (mouseDeltaX<0)	{	//moving left
				stretchPreview.nXOriginSrc=mouseDeltaX*-1;
				stretchPreview.nXOriginDest=0;
				stretchPreview.nWidthSrc = optionsPreview.width + mouseDeltaX;
				stretchPreview.nWidthDest = optionsPreview.width + mouseDeltaX;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);

			}
			else if (mouseDeltaX>0)	{	//moving right
				stretchPreview.nXOriginSrc=0;
				stretchPreview.nXOriginDest=mouseDeltaX;
				stretchPreview.nWidthSrc = optionsPreview.width - mouseDeltaX;
				stretchPreview.nWidthDest = optionsPreview.width - mouseDeltaX;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);
			}


			if (mouseDeltaY>0)	{	//moving down
				stretchPreview.nYOriginSrc=mouseDeltaY*-1;
				stretchPreview.nYOriginDest=0;
				stretchPreview.nHeightSrc = optionsPreview.height + mouseDeltaY;
				stretchPreview.nHeightDest = optionsPreview.height + mouseDeltaY;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);

			}
			else if (mouseDeltaY<0)	{	//moving up
				stretchPreview.nYOriginSrc=0;
				stretchPreview.nYOriginDest=mouseDeltaY;
				stretchPreview.nHeightSrc = optionsPreview.height - mouseDeltaY;
				stretchPreview.nHeightDest = optionsPreview.height - mouseDeltaY;
				stretchPreview.useStretch = TRUE;

				InvalidateRect(hwndPreview, NULL, 0);
			}





//			previewOriginalPoint.x=mousePoint.x;
//			previewOriginalPoint.y=mousePoint.y;
			UpdateBarsFromNSWE(&optionsPreview.nswe);
			UpdateEditboxesFromOptions(&optionsPreview);
			UpdateExportAspectRatioFromOptions(&optionsPreview,0);


		break;
		case WM_LBUTTONUP:
			if (mouseDragPreview == FALSE)
				return 1;
			mouseDragPreview=FALSE;
			ReleaseCapture();
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
		break;


	}

	return 0;
}

int HandlePreviewMousewheel(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	signed int zDelta;
	double longitude;
	double latitude;

	double longspan;
	double latspan;

	double zoomfactor;

	POINT mousePoint;

	SetFocus(hwnd);
	zDelta = HIWORD(wParam);

	if (zDelta>32768)	{//convert it to a signed int
		zDelta -=65536;
	}

	zoomfactor = 12/(double)zDelta;

	longspan = optionsPreview.nswe.east - optionsPreview.nswe.west;
	latspan = optionsPreview.nswe.north - optionsPreview.nswe.south;

	if ((zDelta>0)&&((longspan<0.01) || (latspan<0.01)))	{	//if we're too zoomed in already (i.e. there's less than 0.01 degrees between sides)
		return 0;
	}


	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);
	ScreenToClient(hwnd, &mousePoint);


	longitude = (double)mousePoint.x*longspan/optionsPreview.width + optionsPreview.nswe.west;
	latitude = optionsPreview.nswe.north - latspan* (double)mousePoint.y/(double)optionsPreview.height;

	//printf("\r\nInitial longspan: %f, latspan %f, aspect ratio: %f\r\n", longspan, latspan, longspan/latspan);
	//printf("\r\nmw x%i, y%i ht:%i: wt:%i long:%f,lat:%f, zoom:%f\r\n",mousePoint.x, mousePoint.y,  optionsPreview.height, optionsPreview.width, longitude, latitude, zoomfactor);

	//This shifts the origin to where the mouse was
	optionsPreview.nswe.west-=longitude;
	optionsPreview.nswe.north-=latitude;
	optionsPreview.nswe.east-=longitude;
	optionsPreview.nswe.south-=latitude;
	//resizes
	optionsPreview.nswe.west*=(1-zoomfactor);
	optionsPreview.nswe.north*=(1-zoomfactor);
	optionsPreview.nswe.east*=(1-zoomfactor);
	optionsPreview.nswe.south*=(1-zoomfactor);
	//then moves the origin back
	optionsPreview.nswe.west+=longitude;
	optionsPreview.nswe.north+=latitude;
	optionsPreview.nswe.east+=longitude;
	optionsPreview.nswe.south+=latitude;

	//Now we set the coords for the StretchBlt if required
	if (zoomfactor>0)	{	//if we're zooming in
		//we'll stretch the centre to the whole window
		stretchPreview.nXOriginDest=0;
		stretchPreview.nYOriginDest=0;
		stretchPreview.nWidthDest=optionsPreview.width;
		stretchPreview.nHeightDest=optionsPreview.height;

		//if this isn't already set, it'll be the same as the destination
		if (!stretchPreview.useStretch)	{
			stretchPreview.nXOriginSrc=0;
			stretchPreview.nYOriginSrc=0;
			stretchPreview.nWidthSrc=optionsPreview.width;
			stretchPreview.nHeightSrc=optionsPreview.height;
		}

		stretchPreview.nWidthSrc*=(1-zoomfactor);
		stretchPreview.nHeightSrc*=(1-zoomfactor);

		stretchPreview.nXOriginSrc-=mousePoint.x;
		stretchPreview.nYOriginSrc-=mousePoint.y;

		stretchPreview.nXOriginSrc*=(1-zoomfactor);
		stretchPreview.nYOriginSrc*=(1-zoomfactor);

		stretchPreview.nXOriginSrc+=mousePoint.x;
		stretchPreview.nYOriginSrc+=mousePoint.y;

		stretchPreview.useStretch = TRUE;

		printf("Stretch dest:(%i,%i,%i,%i)", stretchPreview.nXOriginDest, stretchPreview.nYOriginDest, stretchPreview.nWidthDest, stretchPreview.nHeightDest);
		printf(" from src (%i,%i,%i,%i)\r\n", stretchPreview.nXOriginSrc, stretchPreview.nYOriginSrc, stretchPreview.nWidthSrc, stretchPreview.nHeightSrc);
	}
	else	{
		//when zooming out the src wile be the whole window, and we'll compress it to a smaller area
		stretchPreview.nXOriginSrc=0;
		stretchPreview.nYOriginSrc=0;
		stretchPreview.nWidthSrc=optionsPreview.width;
		stretchPreview.nHeightSrc=optionsPreview.height;

		//initially set it up to just the same
		if (!stretchPreview.useStretch)	{
			stretchPreview.nXOriginDest=0;
			stretchPreview.nYOriginDest=0;
			stretchPreview.nWidthDest=optionsPreview.width;
			stretchPreview.nHeightDest=optionsPreview.height;
		}

		//then scale size
		stretchPreview.nWidthDest/=(1-zoomfactor);
		stretchPreview.nHeightDest/=(1-zoomfactor);

		//translating so the origin is at the mouse point
		stretchPreview.nXOriginDest-=mousePoint.x;
		stretchPreview.nYOriginDest-=mousePoint.y;

		stretchPreview.nXOriginDest/=(1-zoomfactor);
		stretchPreview.nYOriginDest/=(1-zoomfactor);

		stretchPreview.nXOriginDest+=mousePoint.x;
		stretchPreview.nYOriginDest+=mousePoint.y;

		stretchPreview.useStretch = TRUE;



	}


	UpdateExportAspectRatioFromOptions(&optionsPreview,0);
	UpdateEditboxesFromOptions(&optionsPreview);
	UpdateBarsFromNSWE(&optionsPreview.nswe);

	SendMessage(hwndPreview, WT_WM_QUEUERECALC , 0,0);

	return 1;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam,LPARAM lParam)
{
	RECT clientRect;

	switch (msg) {
		case WM_CREATE:
			hbmPreview = NULL;
			break;
		case WT_WM_QUEUERECALC:		//start a timer, and send the recalc bitmap when appropriate
			KillTimer(hwnd, IDT_PREVIEWTIMER);
			SetTimer(hwnd, IDT_PREVIEWTIMER, 100, NULL);	//200 is good without threads
			//int w,h;
			GetClientRect(hWndMain, &clientRect);
			PreviewWindowFitToAspectRatio(hWndMain, clientRect.bottom, clientRect.right, (optionsPreview.nswe.east-optionsPreview.nswe.west)/(optionsPreview.nswe.north-optionsPreview.nswe.south));
			InvalidateRect(hwnd, NULL, 0);	//trigger a WM_PAINT, as we may be able to stretch or translate
			return 1;
			break;
		case WM_TIMER:
			printf("timer");
			KillTimer(hwnd, IDT_PREVIEWTIMER);
		case WT_WM_RECALCBITMAP:
			printf("**********recalc******\r\n");
			GetClientRect(hwnd, &clientRect);
			optionsPreview.width = clientRect.right;
			optionsPreview.height = clientRect.bottom;
//			hbmPreview = MakeHBitmapPreview(GetDC(hwnd), &locationHistory);

			hbmQueueSize++;
			CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadSetHBitmap, (LPVOID)hbmQueueSize ,0,NULL));

//			stretchPreview.useStretch = FALSE;
//			InvalidateRect(hwnd, NULL, 0);
    		return 1;
			break;
		case WM_SIZE:	//might have to ensure it doesn't waste time if size doesn't change.
			//printf("***size***\r\n");
			SendMessage(hwndPreview, WT_WM_QUEUERECALC, 0,0);
			InvalidateRect(hwnd, NULL, 0);
			break;
		case WM_PAINT:
			PaintPreview(hwnd, &locationHistory);
			break;
		case WM_LBUTTONDOWN:
		case WM_MOUSEMOVE:
		case WM_LBUTTONUP:
			HandlePreviewLbutton(hwnd, msg, wParam, lParam);
			break;
		case WT_WM_SIGNALMOUSEWHEEL:
			HandlePreviewMousewheel(hwnd, wParam, lParam);
			break;
		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}


int PaintPreview(HWND hwnd, LOCATIONHISTORY * lh)
{
	HDC hdc;
	HDC memDC;
	PAINTSTRUCT ps;
	HGDIOBJ oldBitmap;

	RECT clientRect;
	int width, height;


	GetClientRect(hwnd, &clientRect);
	width=clientRect.right;
	height=clientRect.bottom;


//	optionsPreview.width = clientRect.right;
//	optionsPreview.height = clientRect.bottom;

	if (!previewBM.bitmap)	{
		hdc= BeginPaint(hwnd, &ps);

		DeleteDC(hdc);
		EndPaint(hwnd, &ps);
	}

	hdc= BeginPaint(hwnd, &ps);
	memDC = CreateCompatibleDC(hdc);

	oldBitmap = SelectObject(memDC, hbmPreview);
	if (stretchPreview.useStretch)	{
		SetStretchBltMode(hdc, HALFTONE);
		StretchBlt(hdc, stretchPreview.nXOriginDest, stretchPreview.nYOriginDest, stretchPreview.nWidthDest, stretchPreview.nHeightDest,
			memDC, stretchPreview.nXOriginSrc,stretchPreview.nYOriginSrc,stretchPreview.nWidthSrc, stretchPreview.nHeightSrc, SRCCOPY);

		//fill in the remaining areas
		int top,bottom,left,right;
		top = stretchPreview.nYOriginDest;
		left= stretchPreview.nXOriginDest;
		bottom = stretchPreview.nYOriginDest+stretchPreview.nHeightDest;
		right = stretchPreview.nXOriginDest+stretchPreview.nWidthDest;

		//top panels
		if (top>0)	{
			Rectangle(hdc,0,0, previewBM.width,top);
		}
		if (bottom<previewBM.height)	{
			Rectangle(hdc,0,bottom, previewBM.width, previewBM.height);
		}

		//side panels
		if (left>0)	{
			Rectangle(hdc,0,0, left, previewBM.height);
		}
		if (right<previewBM.width)	{
			Rectangle(hdc,right,0, previewBM.width, previewBM.height);
		}



	}
	else if ((width!=previewBM.width) || (height!=previewBM.height))	{
		printf("width not the same");
		SetStretchBltMode(hdc, HALFTONE);
		StretchBlt(hdc,0,0,width, height, memDC, 0,0,previewBM.width, previewBM.height, SRCCOPY);
	}
	else	{
		BitBlt(hdc,0, 0, width, height, memDC, 0, 0, SRCCOPY);
	}




	SelectObject(memDC, oldBitmap);

	DeleteDC(hdc);
	EndPaint(hwnd, &ps);


	return 0;
}

DWORD WINAPI ThreadSetHBitmap(long queuechit)
{
	if (queuechit<hbmQueueSize)	{
		printf("\r\nQUEUE SKIPPED!!\r\n");
		return 0;
	}

	hbmPreview = MakeHBitmapPreview(GetDC(hwndPreview), &locationHistory, queuechit);
	stretchPreview.useStretch = FALSE;
	InvalidateRect(hwndPreview, NULL, 0);

	return 1;
}


HBITMAP MakeHBitmapPreview(HDC hdc, LOCATIONHISTORY * lh, long queuechit)
{
	HBITMAP bitmap;
	BITMAPINFO bmi;
	BYTE * bits;
	int width, height;
	int x;
	int y;
	COLOUR c;
	COLOUR d;	//window colour

	height = optionsPreview.height;
	width = optionsPreview.width;

	EnterCriticalSection(&critAccessLocations);

	if (queuechit<hbmQueueSize)	{
		LeaveCriticalSection(&critAccessLocations);
		printf("\r\nQUEUE SKIPPED!!\r\n");

		return hbmPreview;
	}

	if (previewBM.bitmap)	{	//if there's already a BM set, destroy it
		bitmapDestroy(&previewBM);
	}

	optionsPreview.colourcycle = (60*60*24);
	optionsPreview.totimestamp =-1;
	optionsPreview.zoom=optionsPreview.width/(optionsPreview.nswe.east-optionsPreview.nswe.west);
	optionsPreview.alpha=200;	//default
	optionsPreview.colourby = COLOUR_BY_TIME;



	//RationaliseOptions(&optionsPreview);
	bitmapInit(&previewBM, &optionsPreview, &locationHistory);
	if (previewBM.lh)	{
		PlotPaths(&previewBM, &locationHistory, &optionsPreview);
	}

	if (hbmPreview!=NULL)	{
		DeleteObject(hbmPreview);
	}

	printf("Creating bitmap %i %i %i %i",width,height, previewBM.width, previewBM.height);
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	bitmap = CreateDIBSection(hdc, &bmi,DIB_RGB_COLORS, &bits, NULL, 0);
	int b=0;
	for (y=0;y<height;y++)	{
		for (x=0;x<width;x++)	{
			c= bitmapPixelGet(&previewBM, x, y);
			d.R=d.G=d.B=0;
			mixColours(&d, &c);
			bits[b] =d.B;	b++;
			bits[b] =d.G;	b++;
			bits[b] =d.R;	b++;
		}
		b=(b+3) & ~3;	//round to next WORD alignment
	}

	LeaveCriticalSection(&critAccessLocations);
	return bitmap;
}



LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	RECT rect;	//temporary rect, currently used for checkingposition of preview window

	switch (msg) {
	case WM_CREATE:
		int y=MARGIN;
		int x=MARGIN;
		//now create child windows
		hwndStaticFilename =CreateWindow("Static","Filename:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		//overview Window
		hwndOverview = CreateWindow("OverviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|WS_CLIPCHILDREN, x, y ,OVERVIEW_WIDTH, OVERVIEW_HEIGHT, hwnd,NULL,hInst,NULL);
		y+=MARGIN+OVERVIEW_HEIGHT;
		//dialog
		hwndStaticNorth = CreateWindow("Static","North:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditNorth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITNORTH, hInst, NULL);
//		y+=MARGIN+TEXT_HEIGHT;
		x+=MARGIN+TEXT_WIDTH_QLONG;
		hwndStaticSouth = CreateWindow("Static","South:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditSouth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITSOUTH, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;

		hwndStaticWest = CreateWindow("Static","West:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditWest = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITWEST, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QLONG;
		//y+=MARGIN+TEXT_HEIGHT;

		hwndStaticEast = CreateWindow("Static","East:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditEast = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QLONG, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEAST, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;
		hwndStaticPreset = CreateWindow("Static", "Preset:",  WS_CHILD | WS_VISIBLE, x,y,TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+ MARGIN;
		hwndEditPreset = CreateWindow("Edit", "Type a place",  WS_CHILD | WS_VISIBLE|WS_BORDER, x,y,TEXT_WIDTH_THREEQUARTERS, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITPRESET, hInst, NULL);


				y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;


		CreateWindow("Static","From:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditDateFrom = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITDATEFROM, hInst, NULL);
//		y+=MARGIN+TEXT_HEIGHT;
		x+=MARGIN+TEXT_WIDTH_QLONG;
		CreateWindow("Static","To:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditDateTo = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITDATETO, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;


		hwndDateSlider = CreateWindow("DateSliderClass",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER|WS_TABSTOP, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT, hwnd, NULL, hInst, NULL);


		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN;
		CreateWindow("Static","Thickness:",	  WS_CHILD | WS_VISIBLE, x,y,TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditThickness = CreateWindow("Edit","1", WS_CHILD | WS_VISIBLE |ES_NUMBER| WS_BORDER|WS_TABSTOP, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITTHICKNESS, hInst, NULL);

		x+=MARGIN+TEXT_WIDTH_QLONG;

//		y+=MARGIN+TEXT_HEIGHT;
//		x=MARGIN;
		CreateWindow("Static","Colour by:",	  WS_CHILD | WS_VISIBLE, x,y,TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);




		//Now the right hand side
		x=MARGIN+OVERVIEW_WIDTH+MARGIN+MARGIN;
		y=MARGIN;
		hwndStaticExportHeading = CreateWindow("Static","Export information", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		hwndStaticExportWidth = CreateWindow("Static","Width:", WS_CHILD | WS_VISIBLE |WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+MARGIN;
		hwndEditExportWidth = CreateWindow("Edit","3600", WS_CHILD | WS_VISIBLE |ES_NUMBER| WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEXPORTWIDTH, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+MARGIN;
		hwndStaticExportHeight = CreateWindow("Static","Height:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+MARGIN;
		hwndEditExportHeight = CreateWindow("Edit","1800", WS_CHILD | WS_VISIBLE |ES_NUMBER| WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEXPORTHEIGHT, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN+OVERVIEW_WIDTH+MARGIN+MARGIN;

		hwndPreview = CreateWindow("PreviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|WS_CLIPCHILDREN, x, y ,OVERVIEW_WIDTH, OVERVIEW_WIDTH, hwnd,NULL,hInst,NULL);

		UpdateEditboxesFromOptions(&optionsPreview);
		break;

	case WM_SIZE:
		SendMessage(hWndStatusbar,msg,wParam,lParam);
		InitializeStatusBar(hWndStatusbar,1);
		PreviewWindowFitToAspectRatio(hwnd, HIWORD(lParam), LOWORD(lParam), (optionsPreview.nswe.east-optionsPreview.nswe.west)/(optionsPreview.nswe.north-optionsPreview.nswe.south));
		break;
	case WM_MENUSELECT:
		return MsgMenuSelect(hwnd,msg,wParam,lParam);
	case WM_MOUSEWHEEL:
		//if it's within the Preview message box, send it there
		GetWindowRect(hwndPreview, &rect);
		printf("Rectleft %i, rectright %i, mousex %i, mousey %i. \r\n",rect.left, rect.right, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if ((GET_X_LPARAM(lParam)>=rect.left) && (GET_X_LPARAM(lParam)<=rect.right) && (GET_Y_LPARAM(lParam)>=rect.top) && (GET_Y_LPARAM(lParam)<=rect.bottom))	{
			SendMessage(hwndPreview, WT_WM_SIGNALMOUSEWHEEL, wParam, lParam);
		}
		return 0;
		break;
	case WM_COMMAND:
		HANDLE_WM_COMMAND(hwnd,wParam,lParam,MainWndProc_OnCommand);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

/*
double PreviewFixParamsToLock(int lockedPart, OPTIONS * o)
{
	int dlat,dlong;

	if ((o->width==0) || (o->height==0))	{

	}

	switch (lockedPart)	{
		case LOCK_AR_WINDOW:
			dlong = o->nswe.east - o->nswe.west;
			dlat = o->nswe.north - o->nswe.south;

			if ((o->nswe.west+dlat*o->aspectratio) > o->nswe.east)
				o->nswe.east = (o->nswe.west+dlat*o->aspectratio);

			else if ((o->nswe.north-dlong/o->aspectratio)>o->nswe.south)
				o->nswe.south =(o->nswe.north-dlong/o->aspectratio);
			break;
	}

	if (o->nswe.south < -90) o->nswe.south=-90;
	if (o->nswe.south > 90) o->nswe.south=90;

	if (o->nswe.north > 90) o->nswe.north=90;
	if (o->nswe.north < -90) o->nswe.north=-90;

	if (o->nswe.west > 180) o->nswe.west=180;
	if (o->nswe.west < -180) o->nswe.west=-180;

	if (o->nswe.east > 180) o->nswe.east=180;
	if (o->nswe.east < -180) o->nswe.east=-180;


	o->zoom=o->width/(o->nswe.east - o->nswe.west);

	UpdateEditboxesFromOptions(&optionsPreview);
	UpdateBarsFromOptions(&optionsPreview);


	return 1;
}
*/

int UpdateExportAspectRatioFromOptions(OPTIONS * o, int forceHeight)
{
	char buffer[255];
	int exportHeight;
	int exportWidth;

	if ((o->width<1)||(o->height<1))
		return 0;

	if (forceHeight)	{
		GetWindowText(hwndEditExportHeight, &buffer[0], 255);
		exportHeight=atol(&buffer[0]);
		printf("height: %i, ",exportHeight);
		exportWidth=exportHeight*o->width/o->height;
		printf("width: %i\r\n",exportWidth);
		sprintf(&buffer[0], "%i", exportWidth);
		SetWindowText(hwndEditExportWidth, &buffer[0]);

	}
	else	{	//we'll be forcing the width static
		GetWindowText(hwndEditExportWidth, &buffer[0], 255);
		exportWidth=atol(&buffer[0]);
		exportHeight=exportWidth*o->height/o->width;
		sprintf(&buffer[0], "%i", exportHeight);
		SetWindowText(hwndEditExportHeight, &buffer[0]);
	}
	InvalidateRect(hwndEditExportWidth, NULL, 0);
	InvalidateRect(hwndEditExportHeight, NULL, 0);
	return 1;
}



int PreviewWindowFitToAspectRatio(HWND hwnd, int mainheight, int mainwidth, double aspectratio)
{
	//int mainheight, mainwidth;
	int availableheight, availablewidth;
	int endheight, endwidth;

	RECT statusbarRect;
	RECT previewRect;
	POINT previewPoint;

	//mainheight = HIWORD(lParam);
	//mainwidth = LOWORD(lParam);

	GetWindowRect(hwndPreview, &previewRect);
	previewPoint.x=previewRect.left;
	previewPoint.y=previewRect.top;
	ScreenToClient(hwnd, &previewPoint);

	//Get status bar heigh
	GetClientRect(hWndStatusbar, &statusbarRect);

	availableheight = mainheight - previewPoint.y - MARGIN-statusbarRect.bottom;
	availablewidth  = mainwidth - previewPoint.x - MARGIN;



	//printf("ht:%i avail:%i top:%i %i\r\n", mainheight, availableheight,previewRect.top, previewPoint.y);

	if (aspectratio == 0)	{	//fit to screen
		endwidth =  availablewidth;
		endheight = availableheight;
		if (endwidth<320) endwidth=320;
	}
	else	{
		if (availableheight*aspectratio > availablewidth)	{	//width limited
			endheight = availablewidth/aspectratio;
			endwidth =  availablewidth;
		} else	{
			endwidth =  availableheight*aspectratio;
			endheight = availableheight;
		}

		if (endwidth<360)	{
			endwidth=360;
			endheight = availablewidth/aspectratio;
		}
	}

	optionsPreview.width=endwidth;
	optionsPreview.height=endheight;
	optionsPreview.aspectratio=aspectratio;

	SetWindowPos(hwndPreview,0,0,0,endwidth,endheight,SWP_NOMOVE|SWP_NOOWNERZORDER);


	printf("ht: %i, wt: %i\r\n", endheight, endwidth);

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;

	hInst = hInstance;
	if (!InitApplication())
		return 0;
	InitializeCriticalSection(&critAccessLocations);
	hAccelTable = LoadAccelerators(hInst,MAKEINTRESOURCE(IDACCEL));
	hWndMain = CreateWindow("worldtrackerWndClass","WorldTracker", WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,		CW_USEDEFAULT,0,CW_USEDEFAULT,0,		NULL,		NULL,		hInst,		NULL);
	CreateSBar(hWndMain,"Ready",1);
	ShowWindow(hWndMain,SW_SHOW);
	while (GetMessage(&msg,NULL,0,0)) {
		if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}



LRESULT CALLBACK DateSliderWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{

	switch (msg) {
		case WM_PAINT:
			return PaintDateSlider(hwnd, &locationHistory, &optionsPreview);
		break;
		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}

	return 0;

}

int PaintDateSlider(HWND hwnd, LOCATIONHISTORY * lh, OPTIONS *o)
{
    PAINTSTRUCT ps;
    HDC hdc;
	char text[64];
	RECT clientRect;

	int x;
	unsigned long secondsspan;
	double	spp;	//seconds per pixel
	time_t	s;
	struct tm *time;	//this is stored statically, so not freed
	int	oldyear,oldmonth;

	GetClientRect(hwnd, &clientRect);

	hdc = BeginPaint(hwnd, &ps);
    sprintf(text, "w:%i", ps.rcPaint.right);
	TextOut(hdc, 0, 0, text, strlen(text));


	secondsspan = lh->latesttimestamp-lh->earliesttimestamp;
	spp=secondsspan/clientRect.right;


	s=lh->earliesttimestamp;
	time=localtime(&s);
	oldyear=time->tm_year;
	oldmonth=time->tm_mon;


	for (x=0;x<clientRect.right;x++)	{
		s=lh->earliesttimestamp+x*spp;

		time=localtime(&s);
		if (time->tm_year!=oldyear)	{
			MoveToEx(hdc, x, 0, (LPPOINT) NULL);
        	LineTo(hdc, x, clientRect.bottom);
			oldyear=time->tm_year;
			oldmonth=time->tm_mon;
		}
		else if	(time->tm_mon!=oldmonth)	{
			MoveToEx(hdc, x, 0, (LPPOINT) NULL);
        	LineTo(hdc, x, clientRect.bottom/2);
			oldmonth=time->tm_mon;
		}
	}

    EndPaint(hwnd, &ps);

	return 0;
}

