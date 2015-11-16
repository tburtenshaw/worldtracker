#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include <d3d8.h>
#include "worldtrackerres.h"
#include "mytrips.h"


#define OVERVIEW_WIDTH 360
#define OVERVIEW_HEIGHT 180
#define TEXT_HEIGHT 20
#define TEXT_DEFAULT_WIDTH 110
#define MARGIN 10

#define ID_EDITNORTH 1001
#define ID_EDITSOUTH 1002
#define ID_EDITWEST 1003
#define ID_EDITEAST 1004


LOCATIONHISTORY * locationHistory;	//note this is different than the command line program which doesn't use a pointer
HINSTANCE hInst;		// Instance handle
HWND hWndMain;		//Main window handle
HWND hwndOverview;

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


HBITMAP hbmOverview;	//this bitmap is generated when the overview is updated.


BM mainBM;
OPTIONS options;


LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK MapWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
HWND  hWndStatusbar;

HBITMAP MakeHBitmapOverview(HWND hwnd, HDC hdc, LOCATIONHISTORY * lh);


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
			GetFileName(JSONfilename,sizeof(JSONfilename));
			memset(&options, 0, sizeof(options));	//set all the option results to 0
			memset(&mainBM, 0, sizeof(mainBM));
			if (locationHistory!=NULL)	//free any old one
				free(locationHistory);
			locationHistory = malloc(sizeof(locationHistory));
			LoadLocations(locationHistory, JSONfilename);
			options.height=180;options.width=360;
			RationaliseOptions(&options);
			bitmapInit(&mainBM, &options, locationHistory);
			PlotPaths(&mainBM, locationHistory, &options);

			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), locationHistory);
			InvalidateRect(hwndOverview,NULL, 0);

			char buffer[256];
			sprintf(buffer,"%s", options.jsonfilenamefinal);
			SetWindowText(hwndStaticFilename, buffer);

			sprintf(buffer,"%f", options.north);
			SetWindowText(hwndEditNorth, buffer);
			sprintf(buffer,"%f", options.south);
			SetWindowText(hwndEditSouth, buffer);
			sprintf(buffer,"%f", options.west);
			SetWindowText(hwndEditWest, buffer);
			sprintf(buffer,"%f", options.east);
			SetWindowText(hwndEditEast, buffer);

		break;

		case IDM_SAVE:
			printf("Writing to %s.\r\n", mainBM.options->kmlfilenamefinal);
			WriteKMLFile(&mainBM);
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
							options.north=a;
						break;
						case ID_EDITSOUTH:
						if ((a >= -90) && (a<=90))
							options.south=a;
						break;
						case ID_EDITWEST:
						if ((a >= -180) && (a<=180))	{
							options.west=a;
							printf("west %f", a);
						}
						break;
						case ID_EDITEAST:
						if ((a >= -180) && (a<=180))
							options.east=a;
						break;
					}
					InvalidateRect(hwndOverview, NULL, FALSE);
					break;
			}
		break;
	}
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

	printf("hwnd: %i, hdc: %i\r\n",hwnd,hdc);
	bitmap = LoadImage(hInst, MAKEINTRESOURCE(IDB_OVERVIEW), IMAGE_BITMAP, 0,0,LR_DEFAULTSIZE|LR_CREATEDIBSECTION);
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);

	if (lh==NULL)	return bitmap;

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
			c= bitmapPixelGet(&mainBM, x,y);
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

	MoveToEx(hdc, 0, 90-options.north, NULL);
	LineTo(hdc, OVERVIEW_WIDTH, 90-options.north);
	MoveToEx(hdc, 0, 90-options.south, NULL);
	LineTo(hdc, OVERVIEW_WIDTH, 90-options.south);

	MoveToEx(hdc, options.west+180, 0, NULL);
	LineTo(hdc, options.west+180, OVERVIEW_HEIGHT);
	MoveToEx(hdc, options.east+180, 0, NULL);
	LineTo(hdc, options.east+180, OVERVIEW_HEIGHT);



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
			hbmOverview = MakeHBitmapOverview(hwnd, GetDC(hwnd), locationHistory);
			break;

		case WM_PAINT:
			DrawMap(hwnd, locationHistory);
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
		//make things null
		locationHistory = NULL;
		int y=MARGIN;
		//now create child windows
		hwndStaticFilename =CreateWindow("Static","Filename:", WS_CHILD | WS_VISIBLE | WS_BORDER, MARGIN, y, TEXT_DEFAULT_WIDTH*3+MARGIN*2, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;
		//overview Window
		hwndOverview = CreateWindow("MapClass", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER, MARGIN, y ,OVERVIEW_WIDTH, OVERVIEW_HEIGHT, hwnd,NULL,hInst,NULL);
		y+=MARGIN+OVERVIEW_HEIGHT;
		//dialog
		hwndStaticNorth = CreateWindow("Static","North:", WS_CHILD | WS_VISIBLE | WS_BORDER, MARGIN, y, TEXT_DEFAULT_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		hwndEditNorth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, TEXT_DEFAULT_WIDTH+MARGIN*2, y, TEXT_DEFAULT_WIDTH, 20, hwnd, (HMENU)ID_EDITNORTH, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;

		hwndStaticSouth = CreateWindow("Static","South:", WS_CHILD | WS_VISIBLE | WS_BORDER, MARGIN, y, TEXT_DEFAULT_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		hwndEditSouth = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, TEXT_DEFAULT_WIDTH+MARGIN*2, y, TEXT_DEFAULT_WIDTH, 20, hwnd, (HMENU)ID_EDITSOUTH, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;

		hwndStaticWest = CreateWindow("Static","West:", WS_CHILD | WS_VISIBLE | WS_BORDER, MARGIN, y, TEXT_DEFAULT_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		hwndEditWest = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, TEXT_DEFAULT_WIDTH+MARGIN*2, y, TEXT_DEFAULT_WIDTH, 20, hwnd, (HMENU)ID_EDITWEST, hInst, NULL);
		y+=MARGIN+TEXT_HEIGHT;

		hwndStaticEast = CreateWindow("Static","East:", WS_CHILD | WS_VISIBLE | WS_BORDER, MARGIN, y, TEXT_DEFAULT_WIDTH, TEXT_HEIGHT, hwnd, 0, hInst, NULL);
		hwndEditEast = CreateWindow("Edit",NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, TEXT_DEFAULT_WIDTH+MARGIN*2, y, TEXT_DEFAULT_WIDTH, TEXT_HEIGHT, hwnd, (HMENU)ID_EDITEAST, hInst, NULL);
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
	hWndMain = CreateWindow("worldtrackerWndClass","worldtracker", WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,		CW_USEDEFAULT,0,CW_USEDEFAULT,0,		NULL,		NULL,		hInst,		NULL);
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

