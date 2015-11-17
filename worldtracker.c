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



#define ID_EDITNORTH 1001
#define ID_EDITSOUTH 1002
#define ID_EDITWEST 1003
#define ID_EDITEAST 1004


LOCATIONHISTORY locationHistory;
HINSTANCE hInst;		// Instance handle
HWND hWndMain;		//Main window handle
HWND hwndOverview;

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


HBITMAP hbmOverview;	//this bitmap is generated when the overview is updated.


BM overviewBM;
//BM preview BM;


OPTIONS optionsOverview;
//OPTIONS optionsPreview;

LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK MapWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh);

int ExportKMLDialogAndComplete(HWND hwnd, OPTIONS * o, LOCATIONHISTORY *lh);

void UpdateStatusBar(LPSTR lpszStatusString, WORD partNumber, WORD displayFlags)
{
    SendMessage(hWndStatusbar,
                SB_SETTEXT,
                partNumber | displayFlags,
                (LPARAM)lpszStatusString);
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

	memset(&locationHistory,0,sizeof(locationHistory));





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

	//The Map window class
	memset(&wc,0,sizeof(WNDCLASS));
	wc.style = CS_HREDRAW|CS_VREDRAW |CS_DBLCLKS ;
	wc.lpfnWndProc = (WNDPROC)MapWndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = "MapClass";
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

			char buffer[256];
			sprintf(buffer,"%s", optionsOverview.jsonfilenamefinal);
			SetWindowText(hwndStaticFilename, buffer);

			sprintf(buffer,"%f", optionsOverview.north);
			SetWindowText(hwndEditNorth, buffer);
			sprintf(buffer,"%f", optionsOverview.south);
			SetWindowText(hwndEditSouth, buffer);
			sprintf(buffer,"%f", optionsOverview.west);
			SetWindowText(hwndEditWest, buffer);
			sprintf(buffer,"%f", optionsOverview.east);
			SetWindowText(hwndEditEast, buffer);

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
			switch (codeNotify)	{
				case EN_CHANGE:
					char szText[128];
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
						}
						break;
						case ID_EDITEAST:
						if ((a >= -180) && (a<=180))
							optionsOverview.east=a;
						break;
					}
					InvalidateRect(hwndOverview, NULL, FALSE);
					break;
			}
		break;
	}
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

	printf("getDIBits1: %i\r\n", result);
	printf("bi.height: %i\t", bi.bmiHeader.biHeight);
	printf("bi.bitcount: %i\t", bi.bmiHeader.biBitCount);
	printf("bi.sizeimage: %i\t\r\n", bi.bmiHeader.biSizeImage);


	bits=malloc(bi.bmiHeader.biSizeImage);

	result = GetDIBits(hdc, bitmap, 0, 180, bits, &bi, DIB_RGB_COLORS);	//get the bitmap location
	printf("getDIBits2: %i\r\n", result);


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
	printf("setDIBits: %i", result);
	free(&bits);


	return bitmap;
}

int DrawMap(HWND hwnd, LOCATIONHISTORY * lh)
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

LRESULT CALLBACK MapWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			hbmOverview = NULL;
			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), &locationHistory);
			break;

		case WM_PAINT:
			DrawMap(hwnd, &locationHistory);
			break;
		default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
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
		hwndOverview = CreateWindow("MapClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, x, y ,OVERVIEW_WIDTH, OVERVIEW_HEIGHT, hwnd,NULL,hInst,NULL);
		y+=MARGIN+OVERVIEW_HEIGHT;
		//dialog
		hwndStaticNorth = CreateWindow("Static","North:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditNorth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITNORTH, hInst, NULL);
//		y+=MARGIN+TEXT_HEIGHT;
		x+=MARGIN+TEXT_WIDTH_QLONG;
		hwndStaticSouth = CreateWindow("Static","South:", WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QSHORT, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		x+=MARGIN+TEXT_WIDTH_QSHORT;
		hwndEditSouth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, TEXT_WIDTH_QLONG, 20, hwnd, (HMENU)ID_EDITSOUTH, hInst, NULL);
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

