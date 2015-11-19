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


LOCATIONHISTORY locationHistory;
HINSTANCE hInst;		// Instance handle
HWND hWndMain;		//Main window handle
HWND hwndOverview;
HWND hwndPreview;

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


OPTIONS optionsOverview;
OPTIONS optionsPreview;

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK PreviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh);
HBITMAP MakeHBitmapPreview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh, int forceRedraw);

int PaintOverview(HWND hwnd, LOCATIONHISTORY * lh);
int PaintPreview(HWND hwnd, LOCATIONHISTORY * lh);

int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh);
int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

int UpdateHWNDsFromOptions(OPTIONS * o);

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
	wc.style = CS_HREDRAW|CS_VREDRAW |CS_DBLCLKS ;
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
	wc.style = CS_HREDRAW|CS_VREDRAW |CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)OverviewWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = "OverviewClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	if (!RegisterClass(&wc))
		return 0;

	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_HREDRAW|CS_VREDRAW |CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)PreviewWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = "PreviewClass";
	wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
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
			UpdateHWNDsFromOptions(&optionsOverview);
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

int UpdateHWNDsFromOptions(OPTIONS * o)
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

int HandleEditControls(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	char szText[128];

	switch (codeNotify)	{
	case EN_CHANGE:
		InvalidateRect(hwndPreview, NULL, 0);
		SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
		double a=atof(szText);
			switch (id)	{
			case ID_EDITNORTH:
				if ((a >= -90) && (a<=90))
					optionsOverview.north=a;
			break;
			case ID_EDITSOUTH:
				if ((a >= -90) && (a<=90))
					optionsOverview.south=a;
			break;
			case ID_EDITWEST:
				if ((a >= -180) && (a<=180))	{
					optionsOverview.west=a;
					printf("west %f", a);
					InvalidateRect(hwnd, NULL, TRUE);
				}
			break;
			case ID_EDITEAST:
				if ((a >= -180) && (a<=180))
					optionsOverview.east=a;
			break;
		}

	case EN_KILLFOCUS:
		switch (id)	{
			case ID_EDITPRESET:
				SendMessage(hwndCtl, WM_GETTEXT, 128,(long)&szText[0]);
				LoadPreset(&optionsOverview, szText);
				UpdateHWNDsFromOptions(&optionsOverview);
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

	//Create export options
	memset(&optionsExport, 0, sizeof(optionsExport));
	optionsExport.width=1000;
	optionsExport.height=1000;
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

	//for the NSWE lines
	HPEN hPen;
	HPEN hPenOld;

	hdc= BeginPaint(hwnd, &ps);
	memDC = CreateCompatibleDC(hdc);

	oldBitmap = SelectObject(memDC, hbmOverview);
	BitBlt(hdc,0, 0, 360, 180, memDC, 0, 0, SRCCOPY);
	SelectObject(memDC, oldBitmap);


	hPen = CreatePen(PS_SOLID, 1, RGB(192,192,192));
	hPenOld = (HPEN)SelectObject(hdc, hPen);

	MoveToEx(hdc, 0, 90-optionsOverview.north, NULL);
	LineTo(hdc, OVERVIEW_WIDTH, 90-optionsOverview.north);
	MoveToEx(hdc, 0, 90-optionsOverview.south, NULL);
	LineTo(hdc, OVERVIEW_WIDTH, 90-optionsOverview.south);

	MoveToEx(hdc, optionsOverview.west+180, 0, NULL);
	LineTo(hdc, optionsOverview.west+180, OVERVIEW_HEIGHT);
	MoveToEx(hdc, optionsOverview.east+180, 0, NULL);
	LineTo(hdc, optionsOverview.east+180, OVERVIEW_HEIGHT);



	SelectObject(hdc, hPenOld);
	DeleteObject(hPen);


	DeleteDC(hdc);
	EndPaint(hwnd, &ps);

	return 0;
}

LRESULT CALLBACK OverviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			hbmOverview = NULL;		//set to null, as it deletes the object if not
			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), &locationHistory);
			break;

		case WM_PAINT:
			PaintOverview(hwnd, &locationHistory);
			break;
		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			hbmPreview = NULL;
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

	GetClientRect(hwnd, &clientRect);
	width=clientRect.right-clientRect.left;
	height=clientRect.bottom-clientRect.top;

	//Set the size from the window, if it's changed, definitely needs redraw
	if (optionsPreview.width != width)	{
		optionsPreview.width = width;
		needsRedraw = 1;
	}

	if (optionsPreview.height =! height)	{
		optionsPreview.height = height;
		needsRedraw = 1;
	}

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
		//printf("doesn't need redraw\r\n");
		return hbmPreview;
	}

	printf("starting width: %i\r\n", width);
	if (needsRedraw)	{	//we'll need to delete the existing bitmap, and generate another
		printf("redraw\r\n");
		if (previewBM.bitmap)	{
			bitmapDestroy(&previewBM);
		}
		RationaliseOptions(&optionsPreview);
		optionsPreview.width =width;
		height = optionsPreview.height;
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
			bits[b] =d.R+x%4*(16*y/height);	b++;
		}
		b+=(4-b%4);	//this rounds to next WORD
	}


	//SetWindowPos(hwnd, 0, clientRect.left, clientRect.top, width, height, SWP_NOMOVE);
	//InvalidateRect(hwnd, NULL, FALSE);
	//printf("New HBM %i\r\n", bitmap);
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
		hwndOverview = CreateWindow("OverviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, x, y ,OVERVIEW_WIDTH, OVERVIEW_HEIGHT, hwnd,NULL,hInst,NULL);
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

		hwndPreview = CreateWindow("PreviewClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, x, y ,OVERVIEW_WIDTH, OVERVIEW_WIDTH, hwnd,NULL,hInst,NULL);


		break;

	case WM_SIZE:
		SendMessage(hWndStatusbar,msg,wParam,lParam);
		InitializeStatusBar(hWndStatusbar,1);
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

