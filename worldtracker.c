#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include <d3d8.h>
#include "worldtrackerres.h"
#include "mytrips.h"
#include "lodepng.h"



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

//these don't really need to be the bearing, but I thought it would be easier
#define D_NORTH 0
#define D_SOUTH 180
#define D_WEST 270
#define D_EAST 90

typedef struct sMovebar MOVEBARINFO;

struct sMovebar	{
	int direction;
	float position;
	COLORREF colour;
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

HBITMAP hbmOverview;	//this bitmap is generated when the overview is updated.
HBITMAP hbmPreview;

BM overviewBM;
BM previewBM;

POINT mousePoint;
BOOL mouseDrag;

OPTIONS optionsOverview;
OPTIONS optionsPreview;

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK PreviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK OverviewMovebarWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

int PreviewWindowFitToAspectRatio(HWND hwnd, LPARAM lParam, double aspectratio);	//aspect ratio here is width/height


HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh);
HBITMAP MakeHBitmapPreview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh, int forceRedraw);

int PaintOverview(HWND hwnd, LOCATIONHISTORY * lh);
int PaintPreview(HWND hwnd, LOCATIONHISTORY * lh);

int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh);
int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

int HandlePreviewMousewheel(HWND hwnd, WPARAM wParam,LPARAM lParam);

int UpdateEditboxesFromOptions(OPTIONS * o);
int UpdateBarsFromOptions(OPTIONS * o);

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
	wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
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
	wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
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
	wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
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
			memset(&optionsOverview, 0, sizeof(optionsOverview));	//set all the option results to 0
			memset(&overviewBM, 0, sizeof(overviewBM));
			printf("Freeing Locations");
			FreeLocations(&locationHistory);	//first free any locations

			LoadLocations(&locationHistory, JSONfilename);
			optionsOverview.height=180;optionsOverview.width=360;
			RationaliseOptions(&optionsOverview);
			bitmapInit(&overviewBM, &optionsOverview, &locationHistory);
			PlotPaths(&overviewBM, &locationHistory, &optionsOverview);
			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), &locationHistory);
			InvalidateRect(hwndOverview,NULL, 0);
			InvalidateRect(hwndPreview, NULL, 0);
			UpdateEditboxesFromOptions(&optionsOverview);
			UpdateBarsFromOptions(&optionsOverview);
		break;

		case IDM_SAVE:
			ExportKMLDialogAndComplete(hwnd, &optionsOverview, &locationHistory);
		break;

		case IDM_EXIT:
		PostMessage(hwnd,WM_CLOSE,0,0);
		break;

		case ID_EDITNORTH:
		case ID_EDITSOUTH:
		case ID_EDITEAST:
		case ID_EDITWEST:
		case ID_EDITPRESET:
			HandleEditControls(hwnd, id, hwndCtl, codeNotify);
		break;
	}
}

int UpdateEditboxesFromOptions(OPTIONS * o)
{
	char buffer[256];
	sprintf(buffer,"%s", o->jsonfilenamefinal);
	SetWindowText(hwndStaticFilename, buffer);

	sprintf(buffer,"%f", o->north);
	SetWindowText(hwndEditNorth, buffer);
	sprintf(buffer,"%f", o->south);
	SetWindowText(hwndEditSouth, buffer);
	sprintf(buffer,"%f", o->west);
	SetWindowText(hwndEditWest, buffer);
	sprintf(buffer,"%f", o->east);
	SetWindowText(hwndEditEast, buffer);

	return 0;
}

int UpdateBarsFromOptions(OPTIONS * o)
{
	MoveWindow(hwndOverviewMovebarWest, 180 + o->west , 0, 3,180,TRUE);
	MoveWindow(hwndOverviewMovebarEast, 180 + o->east , 0, 3,180,TRUE);
	MoveWindow(hwndOverviewMovebarNorth, 0, 90 - o->north, 360,3,TRUE);
	MoveWindow(hwndOverviewMovebarSouth, 0, 90 - o->south, 360,3,TRUE);
	return 0;
}

int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	char szText[128];
	double value;	//to hold the textbox value

	switch (codeNotify)	{
	case EN_CHANGE:
		InvalidateRect(hwndPreview, NULL, 0);
		SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
		value=atof(szText);
		switch (id)	{
		case ID_EDITNORTH:
			if ((value >= -90) && (value<=90))
				optionsOverview.north=value;
			break;
			case ID_EDITSOUTH:
				if ((value >= -90) && (value<=90))
					optionsOverview.south=value;
			break;
			case ID_EDITWEST:
				if ((value >= -180) && (value<=180))	{
					optionsOverview.west=value;

				}
			break;
			case ID_EDITEAST:
				if ((value >= -180) && (value<=180))
					optionsOverview.east=value;
			break;
		}
		UpdateBarsFromOptions(&optionsOverview);
	break;
	case EN_KILLFOCUS:
		switch (id)	{
			case ID_EDITPRESET:
				SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
				LoadPreset(&optionsOverview, szText);
				UpdateEditboxesFromOptions(&optionsOverview);
				UpdateBarsFromOptions(&optionsOverview);
				InvalidateRect(hwndOverview, NULL, FALSE);
				break;
		}
	break;
	}


	return 0;
}


int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh)
{
	OPTIONS optionsExport;
	BM exportBM;
	int error;
	char editboxtext[128];

	//Create export options
	memset(&optionsExport, 0, sizeof(optionsExport));

	SendMessage(hwndEditExportWidth, WM_GETTEXT, 128,(long)&editboxtext[0]);
	optionsExport.width=atol(&editboxtext[0]);
	SendMessage(hwndEditExportWidth, WM_GETTEXT, 128,(long)&editboxtext[0]);
	optionsExport.height=atol(&editboxtext[0]);
	//get the NSWE from the overview
	optionsExport.north=o->north;
	optionsExport.south=o->south;
	optionsExport.west=o->west;
	optionsExport.east=o->east;

	memcpy(&optionsExport.kmlfilenamefinal, &o->kmlfilenamefinal, sizeof(o->kmlfilenamefinal));
	memcpy(&optionsExport.pngfilenamefinal, &o->pngfilenamefinal, sizeof(o->kmlfilenamefinal));
	RationaliseOptions(&optionsExport);

	bitmapInit(&exportBM, &optionsExport, &locationHistory);
	DrawGrid(&exportBM);
	PlotPaths(&exportBM, &locationHistory, &optionsExport);

	printf("png:%s, w:%i, h:%i size:%i\r\n",exportBM.options->pngfilenamefinal, exportBM.width, exportBM.height, exportBM.sizebitmap);
	error = lodepng_encode32_file(exportBM.options->pngfilenamefinal, exportBM.bitmap, exportBM.width, exportBM.height);
	if(error) fprintf(stderr, "LodePNG error %u: %s\n", error, lodepng_error_text(error));

//	bitmapWrite(&exportBM, "test.raw");
	//Write KML file
	fprintf(stdout, "KML to %s. PNG to %s\r\n", exportBM.options->kmlfilenamefinal, exportBM.options->pngfilenamefinal);
	WriteKMLFile(&exportBM);
	bitmapDestroy(&exportBM);


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

	result = GetDIBits(hdc,bitmap,0,180, NULL, &bi,DIB_RGB_COLORS);	//get the info

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
	char buffer[256];

	switch (msg) {
		case WM_LBUTTONDOWN:
			SetFocus(hwnd);	//mainly to get the focus out of the edit boxes
			GetCursorPos(&mousePoint);
			mouseDrag=TRUE;
			SetCapture(hwnd);
		break;
		case WM_MOUSEMOVE:
			if (mouseDrag == FALSE)
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
			if (mouseDrag == FALSE)
				return 1;

			pMbi = (MOVEBARINFO *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			mouseDrag=FALSE;
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

/*
			if ((pMbi->direction == D_WEST)||(pMbi->direction == D_EAST))	{
				pMbi->position=(float)mousePoint.x-180;	//do we even need this position ?is it just duplicating things?
			}	else	{
				pMbi->position=90-(float)mousePoint.y;
			}
*/
			if (pMbi->direction == D_WEST)	{
				optionsOverview.west = mousePoint.x-180;
			}
			if (pMbi->direction == D_EAST)	{
				optionsOverview.east = mousePoint.x-180;
			}
			if (pMbi->direction == D_NORTH)	{
				optionsOverview.north =90-mousePoint.y;
			}
			if (pMbi->direction == D_SOUTH)	{
				optionsOverview.south =90-mousePoint.y;
			}
			UpdateEditboxesFromOptions(&optionsOverview);

/*			sprintf(buffer,"%f",pMbi->position);

			if (pMbi->direction == D_WEST)	{
				SetWindowText(hwndEditWest, buffer);
			}
			if (pMbi->direction == D_EAST)	{
				SetWindowText(hwndEditEast, buffer);
			}
			if (pMbi->direction == D_NORTH)	{
				SetWindowText(hwndEditNorth, buffer);
			}
			if (pMbi->direction == D_SOUTH)	{
				SetWindowText(hwndEditSouth, buffer);
			}
*/
		break;
//		case WM_MOVE:
//			printf("move: %i", lParam);
			//MoveWindow(hwnd, lParam, 0, 3,180,TRUE);
//			return 0;
//		break;
		//case WM_ERASEBKGND:
//    		return 1;
		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}


	return 0;
}

LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
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


			break;
		case WM_ERASEBKGND:
    		return 1;
		case WM_PAINT:
			PaintOverview(hwnd, &locationHistory);
			break;
		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

int HandlePreviewMousewheel(HWND hwnd, WPARAM wParam,LPARAM lParam)
{
	signed int zDelta;
	double longitude;
	double latitude;

	double longspan;
	double latspan;

	double fromw, frome, fromn, froms;
	double zoomfactor;

	zoomfactor=0.1;
	//zoomfactor=-0.1;	//negative zooms in

	POINT mousePoint;

	zDelta = HIWORD(wParam);

	if (zDelta>32768)	{//convert it to a signed int
		zDelta -=65536;
	}

	zoomfactor = 12/(double)zDelta;

	mousePoint.x = GET_X_LPARAM(lParam);
	mousePoint.y = GET_Y_LPARAM(lParam);
	ScreenToClient(hwnd, &mousePoint);


	longspan = optionsOverview.east - optionsOverview.west;
	latspan = optionsOverview.north - optionsOverview.south;

	longitude = (double)mousePoint.x*longspan/optionsOverview.width + optionsOverview.west;
	latitude = optionsOverview.north - latspan* (double)mousePoint.y/(double)optionsOverview.height;

	//printf("\r\nInitial longspan: %f, latspan %f, aspect ratio: %f\r\n", longspan, latspan, longspan/latspan);
	printf("\r\nmw %i: x%i, y%i ht:%i: long:%f,lat:%f\r\n",zDelta,mousePoint.x,mousePoint.y,  optionsOverview.height,longitude, latitude);

	optionsOverview.west-=longitude;
	optionsOverview.north-=latitude;
	optionsOverview.east-=longitude;
	optionsOverview.south-=latitude;

	optionsOverview.west*=(1-zoomfactor);
	optionsOverview.north*=(1-zoomfactor);
	optionsOverview.east*=(1-zoomfactor);
	optionsOverview.south*=(1-zoomfactor);

	optionsOverview.west+=longitude;
	optionsOverview.north+=latitude;
	optionsOverview.east+=longitude;
	optionsOverview.south+=latitude;


	UpdateEditboxesFromOptions(&optionsOverview);
	UpdateBarsFromOptions(&optionsOverview);

	return 0;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{

	switch (msg) {
		case WM_CREATE:
			hbmPreview = NULL;
			hbmPreview = MakeHBitmapPreview(hwnd, GetDC(hwnd), &locationHistory, 1);
			break;
		case WM_ERASEBKGND:
    		return 1;
		case WM_SIZE:
			printf("s");
			hbmPreview = MakeHBitmapPreview(hwnd, GetDC(hwnd), &locationHistory, 1);
			break;
		case WM_PAINT:
			PaintPreview(hwnd, &locationHistory);
			break;
		case WM_LBUTTONDOWN:
			printf("lbutton");
			hbmPreview = MakeHBitmapPreview(hwnd, GetDC(hwnd), &locationHistory, 1);
			InvalidateRect(hwndPreview,NULL, 0);
			break;
		case WM_MOUSEMOVE:
			SetFocus(hwnd);	//this is a temporary move so we get the WM_MOUSEWHEEL messages;
			break;
		case WM_MOUSEWHEEL:
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

	hbmPreview = MakeHBitmapPreview(hwnd, GetDC(hwnd), &locationHistory, 0);

//	GetClientRect(hwnd, &clientRect);
//	width=clientRect.right-clientRect.left;
//	height=clientRect.bottom-clientRect.top;
	width=optionsPreview.width;
	height=optionsPreview.height;

	hdc= BeginPaint(hwnd, &ps);
	memDC = CreateCompatibleDC(hdc);

	oldBitmap = SelectObject(memDC, hbmPreview);
	BitBlt(hdc,0, 0, width, height, memDC, 0, 0, SRCCOPY);
	//printf("blt:%i ",hbmPreview);
	SelectObject(memDC, oldBitmap);

	DeleteDC(hdc);
	EndPaint(hwnd, &ps);


	return 0;
}

HBITMAP MakeHBitmapPreview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh, int forceRedraw)
{
	RECT clientRect;
	HBITMAP bitmap;
	BITMAPINFO bmi;
	BYTE * bits;
	int width, height;
	int x;
	int y;
	COLOUR c;
	COLOUR d;	//window colour

	int needsRedraw;	//whether we need to recalculate the BM

	needsRedraw = 0;

	if (forceRedraw)
		needsRedraw = 1;

	if (hbmPreview==NULL)	{
		needsRedraw = 1;
		printf("hbmPreview is null");
	}

	if (!previewBM.bitmap)	{
		needsRedraw = 1;
		printf("no bitmap");
	}

//	GetClientRect(hwnd, &clientRect);
//	width=clientRect.right-clientRect.left;
//	height=clientRect.bottom-clientRect.top;

	//Set the size from the window, if it's changed, definitely needs redraw
/*
	if (optionsPreview.width != width)	{
		printf("different width %i %i\r\n", optionsPreview.width, width);
		optionsPreview.width = width;
		needsRedraw = 1;
	}

	if (optionsPreview.height =! height)	{
		optionsPreview.height = height;
		printf("different height");
		needsRedraw = 1;
	}
	*/

	//Get the options from the preview
	if (optionsPreview.north != optionsOverview.north)	{
		optionsPreview.north =optionsOverview.north;
		needsRedraw = 1;
	}
	if (optionsPreview.south != optionsOverview.south)	{
		optionsPreview.south =optionsOverview.south;
		needsRedraw = 1;
	}
	if (optionsPreview.west != optionsOverview.west)	{
		optionsPreview.west =optionsOverview.west;
		needsRedraw = 1;
	}
	if (optionsPreview.east != optionsOverview.east)	{
		optionsPreview.east =optionsOverview.east;
		needsRedraw = 1;
	}


	if (needsRedraw==0)	{
		return hbmPreview;
	}

	height = optionsPreview.height;
	width = optionsPreview.width;


	printf("starting width: %i\r\n", width);
	if (needsRedraw)	{	//we'll need to delete the existing bitmap, and generate another
		printf("\r\n **** redraw ****\r\n");
		if (previewBM.bitmap)	{
			bitmapDestroy(&previewBM);
		}
		RationaliseOptions(&optionsPreview);
		//optionsPreview.width =width;
		//height = optionsPreview.height;
		//width = optionsPreview.width;
		bitmapInit(&previewBM, &optionsPreview, &locationHistory);
		PlotPaths(&previewBM, &locationHistory, &optionsPreview);
	}
	printf("Finishing width: %i\r\n", width);

		RationaliseOptions(&optionsPreview);
	if (hbmPreview!=NULL)	{
		DeleteObject(hbmPreview);
		//printf("Deleted old Preview HBITMAP\r\n");
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
		b+=(4-b%4);	//this rounds to next WORD
	}


	optionsOverview.height	=height;

	return bitmap;
}



LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
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

		//Now the right hand side
		x=MARGIN+OVERVIEW_WIDTH+MARGIN+MARGIN;
		y=MARGIN;
		hwndStaticExportHeading = CreateWindow("Static","Export information", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, OVERVIEW_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		hwndStaticExportWidth = CreateWindow("Static","Width:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+MARGIN;
		hwndEditExportWidth = CreateWindow("Edit","3600", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEXPORTWIDTH, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+MARGIN;
		hwndStaticExportHeight = CreateWindow("Static","Height:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=TEXT_WIDTH_QUARTER+MARGIN;
		hwndEditExportHeight = CreateWindow("Edit","1800", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QUARTER, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEXPORTHEIGHT, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		x=MARGIN+OVERVIEW_WIDTH+MARGIN+MARGIN;

		hwndPreview = CreateWindow("PreviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|WS_CLIPCHILDREN, x, y ,OVERVIEW_WIDTH, OVERVIEW_WIDTH, hwnd,NULL,hInst,NULL);
		break;

	case WM_SIZE:
		SendMessage(hWndStatusbar,msg,wParam,lParam);
		InitializeStatusBar(hWndStatusbar,1);

		PreviewWindowFitToAspectRatio(hwnd, lParam, 1);
		break;
	case WM_MENUSELECT:
		return MsgMenuSelect(hwnd,msg,wParam,lParam);
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


int PreviewWindowFitToAspectRatio(HWND hwnd, LPARAM lParam, double aspectratio)
{
	int mainheight, mainwidth;
	int availableheight, availablewidth;
	int endheight, endwidth;

	RECT previewRect;
	POINT previewPoint;

	mainheight = HIWORD(lParam);
	mainwidth = LOWORD(lParam);

	GetWindowRect(hwndPreview, &previewRect);
	previewPoint.x=previewRect.left;
	previewPoint.y=previewRect.top;
	ScreenToClient(hwnd, &previewPoint);

	availableheight = mainheight - previewPoint.y - MARGIN;
	availablewidth  = mainwidth - previewPoint.x - MARGIN;

	//printf("ht:%i avail:%i top:%i %i\r\n", mainheight, availableheight,previewRect.top, previewPoint.y);

	if (availableheight*aspectratio > availablewidth)	{	//width limited
		endheight = availablewidth/aspectratio;
		endwidth =  availablewidth;
	} else	{
		endwidth =  availableheight*aspectratio;
		endheight = availableheight;
	}

	optionsPreview.width=endwidth;
	optionsPreview.height=endheight;


	if (endwidth<360)	{
		endwidth=360;
		endheight = availablewidth/aspectratio;
	}

	int r= SetWindowPos(hwndPreview,0,0,0,endwidth,endheight,SWP_NOMOVE|SWP_NOOWNERZORDER);


	printf("ht: %i, wt: %i r:%i\r\n", endheight, endwidth,r);

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	MSG msg;
	HANDLE hAccelTable;

	hInst = hInstance;
	if (!InitApplication())
		return 0;
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

